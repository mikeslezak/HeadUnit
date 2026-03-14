#include "ClaudeClient.h"
#include "ToolExecutor.h"
#include <QDebug>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QSettings>
#include <QSslSocket>

ClaudeClient::ClaudeClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_model(DEFAULT_MODEL)
    , m_maxTokens(4096)
    , m_temperature(0.3)
    , m_isConnected(false)
    , m_isProcessing(false)
    , m_statusMessage("Not configured")
{
    qDebug() << "ClaudeClient: Initializing...";

    qDebug() << "ClaudeClient: SSL support:" << QSslSocket::supportsSsl();
    qDebug() << "ClaudeClient: SSL library build version:" << QSslSocket::sslLibraryBuildVersionString();

    // Load API key from environment first, fall back to QSettings
    m_apiKey = qEnvironmentVariable("CLAUDE_API_KEY");
    if (m_apiKey.isEmpty()) {
        QSettings settings;
        m_apiKey = settings.value("claude/apiKey", "").toString();
    }

    if (!m_apiKey.isEmpty()) {
        m_isConnected = true;
        setStatusMessage("Ready - API key configured");
        qDebug() << "ClaudeClient: API key loaded";
    } else {
        setStatusMessage("API key required");
        qDebug() << "ClaudeClient: No API key configured";
    }

    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &ClaudeClient::onNetworkReply);

    // Conversation inactivity timeout — clear history after 60s of no messages
    m_conversationTimer = new QTimer(this);
    m_conversationTimer->setSingleShot(true);
    m_conversationTimer->setInterval(5 * 60 * 1000);  // 5 minutes — keep context alive for multi-turn
    connect(m_conversationTimer, &QTimer::timeout, this, [this]() {
        if (m_isProcessing) {
            // Don't wipe history during an active request — retry later
            m_conversationTimer->start();
            return;
        }
        if (!m_conversationHistory.isEmpty()) {
            qDebug() << "ClaudeClient: Conversation timed out, clearing history";
            clearConversation();
        }
    });

    // Safety timeout — force-reset m_isProcessing after 75s
    m_safetyTimer = new QTimer(this);
    m_safetyTimer->setSingleShot(true);
    m_safetyTimer->setInterval(75000);
    connect(m_safetyTimer, &QTimer::timeout, this, [this]() {
        if (m_isProcessing) {
            qWarning() << "ClaudeClient: Safety timeout — forcing processing reset after 75s";
            if (m_currentReply) {
                m_currentReply->abort();
                m_currentReply->deleteLater();
                m_currentReply = nullptr;
            }
            m_streamBuffer.clear();
            m_pendingToolCount = 0;
            m_pendingToolResults.clear();
            m_pendingAssistantContent = QJsonArray();
            m_accumulatedText.clear();
            m_toolGeneration++;
            m_isProcessing = false;
            emit processingChanged();
            sanitizeHistory();
            // Clear stale pending state in ToolExecutor so late signals don't trigger side effects
            if (m_toolExecutor) m_toolExecutor->clearPendingTools();
            emit error("Request timed out");
            setStatusMessage("Error: Request timed out");
        }
    });

    qDebug() << "ClaudeClient: Using model" << m_model;
}

ClaudeClient::~ClaudeClient()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

// ========================================================================
// CONFIGURATION
// ========================================================================

void ClaudeClient::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();
    m_isConnected = !m_apiKey.isEmpty();
    emit connectionChanged();
    setStatusMessage(m_isConnected ? "API key configured" : "API key cleared");
}

void ClaudeClient::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        emit modelChanged();
    }
}

void ClaudeClient::setMaxTokens(int tokens)
{
    tokens = qBound(256, tokens, 4096);
    if (m_maxTokens != tokens) {
        m_maxTokens = tokens;
        emit maxTokensChanged();
    }
}

void ClaudeClient::setTemperature(double temp)
{
    temp = qBound(0.0, temp, 1.0);
    if (m_temperature != temp) {
        m_temperature = temp;
        emit temperatureChanged();
    }
}

// ========================================================================
// MESSAGING
// ========================================================================

void ClaudeClient::sendMessage(const QString &message, const QString &systemContext, bool ephemeral)
{
    if (!m_isConnected || m_apiKey.isEmpty()) {
        emit error("API key not configured");
        return;
    }

    if (message.trimmed().isEmpty()) {
        emit error("Empty message");
        return;
    }

    // Cancel any in-flight request and clean up incomplete tool exchanges
    if (m_isProcessing) {
        qWarning() << "ClaudeClient: Already processing — canceling previous request for new one";
        if (m_currentReply) {
            m_currentReply->abort();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
        m_safetyTimer->stop();
        m_streamBuffer.clear();
        m_pendingToolCount = 0;
        m_pendingToolResults.clear();
        m_accumulatedText.clear();
        m_toolGeneration++;  // Invalidate any stale singleShot timeouts
        m_isProcessing = false;

        // Roll back any incomplete tool exchanges from history.
        // During a tool loop, history may have:
        //   ... user(text) → assistant(tool_use) → user(tool_result) → assistant(tool_use) → ...
        // We need to remove everything after the last valid user(text) message
        // to avoid orphaned tool_use/tool_result pairs.
        sanitizeHistory();
    }

    m_isProcessing = true;
    m_ephemeralRequest = ephemeral;
    m_accumulatedText.clear();
    emit processingChanged();
    setStatusMessage("Sending request to Claude...");

    m_conversationTimer->start();

    // Build and cache system prompt
    m_currentSystemPrompt = buildSystemPrompt();
    if (!systemContext.isEmpty()) {
        m_currentSystemPrompt += "\nLIVE CONTEXT:\n" + systemContext;
    }

    // Store pending user message — added to history only on successful final response
    m_pendingUserMessage = message;

    // Add user message to conversation history for this request
    addToHistory("user", message);

    // Fire the API request
    fireApiRequest();
}

void ClaudeClient::clearConversation()
{
    m_conversationHistory = QJsonArray();
    m_currentResponse.clear();
    emit conversationActiveChanged();
    setStatusMessage("Conversation cleared");
    qDebug() << "ClaudeClient: Conversation history cleared";
}

void ClaudeClient::cancelRequest()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_safetyTimer->stop();
    m_pendingToolCount = 0;
    m_pendingToolResults.clear();
    m_pendingAssistantContent = QJsonArray();
    m_accumulatedText.clear();
    m_streamBuffer.clear();
    m_toolGeneration++;  // Invalidate any stale singleShot timeouts
    m_isProcessing = false;
    emit processingChanged();
    sanitizeHistory();
    setStatusMessage("Request cancelled");
}

// ========================================================================
// SYSTEM CONTEXT
// ========================================================================

void ClaudeClient::setAvailableTools(const QJsonArray &tools)
{
    m_availableTools = tools;
    qDebug() << "ClaudeClient: Available tools set:" << tools.size() << "tools";
}

void ClaudeClient::setToolExecutor(ToolExecutor *executor)
{
    m_toolExecutor = executor;

    // Connect async tool completion signal
    // Use Qt::QueuedConnection to ensure this fires AFTER executeToolsAndContinue finishes
    connect(executor, &ToolExecutor::toolCompleted, this, [this](const QString &toolUseId, const QJsonObject &result) {
        // Guard: ignore stale completions from a canceled/replaced request.
        // m_toolGeneration is incremented on cancel/new-request; if it doesn't match
        // the generation when these tools were dispatched, this completion is stale.
        if (m_activeToolGeneration != m_toolGeneration) {
            qDebug() << "ClaudeClient: Ignoring stale toolCompleted for" << toolUseId << "(generation mismatch)";
            return;
        }
        if (m_pendingToolCount <= 0) {
            qDebug() << "ClaudeClient: Ignoring stale toolCompleted for" << toolUseId << "(no pending tools)";
            return;
        }
        // Guard: ignore duplicate completions (timeout already filled this slot)
        if (m_pendingToolResults.contains(toolUseId)) {
            qDebug() << "ClaudeClient: Ignoring duplicate toolCompleted for" << toolUseId;
            return;
        }

        qDebug() << "ClaudeClient: Async tool completed:" << toolUseId;
        m_pendingToolResults[toolUseId] = result;
        m_pendingToolCount--;

        if (m_pendingToolCount <= 0) {
            submitToolResults();
        }
    }, Qt::QueuedConnection);

    qDebug() << "ClaudeClient: ToolExecutor set";
}

void ClaudeClient::setContactNames(const QStringList &names)
{
    m_contactNames = names;
    qDebug() << "ClaudeClient: Contact names set:" << names.size() << "contacts";
}

// ========================================================================
// NETWORK HANDLING
// ========================================================================

void ClaudeClient::fireApiRequest()
{
    QJsonObject request = buildRequest(m_currentSystemPrompt);

    QUrl url(API_ENDPOINT);
    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("x-api-key", m_apiKey.toUtf8());
    networkRequest.setRawHeader("anthropic-version", API_VERSION);
    networkRequest.setTransferTimeout(60000);
    networkRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    QByteArray requestData = QJsonDocument(request).toJson(QJsonDocument::Compact);
    qDebug() << "ClaudeClient: Sending request:" << requestData.left(300) << "...";

    m_streamBuffer.clear();
    m_currentReply = m_networkManager->post(networkRequest, requestData);

    connect(m_currentReply, &QNetworkReply::readyRead,
            this, &ClaudeClient::onReadyRead);

    m_safetyTimer->start();
}

void ClaudeClient::onNetworkReply(QNetworkReply *reply)
{
    if (reply != m_currentReply) {
        reply->deleteLater();
        return;
    }

    m_safetyTimer->stop();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = m_streamBuffer;
    m_streamBuffer.clear();

    qDebug() << "ClaudeClient: HTTP" << statusCode << "Response size:" << responseData.size();

    if (reply->error() != QNetworkReply::NoError || (statusCode >= 400 && statusCode < 600)) {
        QString errorMsg = reply->errorString();
        if (reply->error() == QNetworkReply::NoError) {
            errorMsg = QString("HTTP %1").arg(statusCode);
        }
        qWarning() << "ClaudeClient: Network error:" << errorMsg << "Status:" << statusCode;
        qWarning() << "ClaudeClient: Response body:" << responseData.left(500);

        m_isProcessing = false;
        m_pendingToolCount = 0;
        m_pendingToolResults.clear();
        m_accumulatedText.clear();
        emit processingChanged();

        // Check for tool_result/tool_use mismatch — conversation history is corrupted
        if (responseData.contains("tool_result") && responseData.contains("tool_use")) {
            qWarning() << "ClaudeClient: Tool ID mismatch — clearing conversation history";
            m_conversationHistory = QJsonArray();
            emit error("Had a hiccup, try again.");
        } else {
            emit error("Network error: " + errorMsg);
            // Clean up history — remove incomplete tool exchanges and the failed user message
            sanitizeHistory();
        }
        setStatusMessage("Error: " + errorMsg);

        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        m_isProcessing = false;
        emit processingChanged();
        emit error("Invalid JSON response");

        sanitizeHistory();

        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    reply->deleteLater();
    m_currentReply = nullptr;

    parseResponse(doc.object());
}

void ClaudeClient::onReadyRead()
{
    if (!m_currentReply) return;
    m_streamBuffer.append(m_currentReply->readAll());
}

// ========================================================================
// REQUEST/RESPONSE
// ========================================================================

QJsonObject ClaudeClient::buildRequest(const QString &systemPrompt)
{
    QJsonObject request;
    request["model"] = m_model;
    request["max_tokens"] = m_maxTokens;
    request["temperature"] = m_temperature;
    request["system"] = systemPrompt;
    request["messages"] = m_conversationHistory;

    if (!m_availableTools.isEmpty()) {
        request["tools"] = m_availableTools;
    }

    return request;
}

QString ClaudeClient::buildSystemPrompt() const
{
    QString prompt;

    // Identity & personality
    prompt += "You are Jarvis, an AI copilot in a truck. ";
    prompt += "You have situational awareness: GPS, weather, vehicle data, and active route. ";
    prompt += "Casual, warm, brief. Talk like a buddy, not a corporate assistant. ";
    prompt += "Keep it short and helpful.\n\n";

    // Voice output rules
    prompt += "VOICE RULES:\n";
    prompt += "- Everything you say is spoken aloud via TTS. NEVER use markdown: no **, ##, *, bullets, numbered lists, backticks, or emojis.\n";
    prompt += "- Keep responses SHORT: 1-2 sentences for simple replies, 3-4 for route briefings.\n";
    prompt += "- Write how someone actually talks. Use contractions.\n";
    prompt += "- For place search results: present ONE place at a time. Example: 'How about Taco Bell? It\\'s about 2 k away, rated 4.2. Want me to take you there?' Then call set_follow_up. If the user says no or next, present the next result the same way. If they say yes, navigate. If you run out of results, say so.\n";
    prompt += "- For route briefings: weave destination, drive time, weather, and conditions into one natural flow.\n\n";

    // Tool use guidance
    prompt += "TOOLS:\n";
    prompt += "- search_places: BROWSE options ('find food', 'gas stations nearby'). Set along_route=true for 'food along the way'. Use near='city name' when user mentions a specific area (e.g. 'find food near Red Deer'). You can combine along_route=true with near to search a specific stretch of the route. Always call set_follow_up after presenting a result.\n";
    prompt += "- navigate: User has DECIDED on a destination. Say ONLY a 2-3 word confirmation like 'On it' or 'You got it' — do NOT say the destination name or anything about the route. The system will automatically deliver a full route briefing with destination, drive time, and conditions a few seconds later.\n";
    prompt += "- add_stop: Add a waypoint/stop along the ACTIVE route. The route is recalculated to go through the stop then continue to the final destination. Use this INSTEAD of navigate when the user picks a place from search results and there is an active route — e.g. 'yeah let's stop there', 'that one', 'yes'. Only use navigate if the user wants to completely change their destination.\n";
    prompt += "- play_music: Search and play. Set type='tracks' for a song, 'albums' for a full album, 'artists' for an artist's top tracks. 'Play Rumours by Fleetwood Mac' → type=albums. 'Play Radiohead' → type=artists. 'Play Bohemian Rhapsody' → type=tracks.\n";
    prompt += "- control_playback: Pause, resume, skip, previous, shuffle, repeat. Also supports seek_ms for seeking ('skip to 2 minutes' → seek_ms=120000).\n";
    prompt += "- music_info: What's currently playing — track, artist, album, quality, position.\n";
    prompt += "- add_favorite: Save/unsave current track to favorites. Set remove=true to unsave.\n";
    prompt += "- call_contact: Call a contact by name.\n";
    prompt += "- hangup_call: End the current call.\n";
    prompt += "- answer_call: Answer an incoming call.\n";
    prompt += "- send_message: Text a contact. System shows confirmation before sending.\n";
    prompt += "- read_messages: Read recent messages from a contact.\n";
    prompt += "- set_follow_up: Keep mic open 12 seconds for reply without wake word. Use after asking a question.\n";
    prompt += "- quiet_mode: Toggle proactive alerts on/off.\n\n";

    prompt += "FOLLOW-UP AFTER SEARCH:\n";
    prompt += "When user says 'yes', 'yeah', 'sure', 'let\\'s go', etc. after you present a place: if there is an active route, use add_stop (keeps the route and adds the place as a stop along the way). If there is NO active route, use navigate. When they say 'no', 'next', 'nah', 'what else', present the next result from the search. You remember all the results from the search.\n\n";

    // Contact names
    if (!m_contactNames.isEmpty()) {
        prompt += "USER'S CONTACTS:\n";
        prompt += m_contactNames.join(", ");
        prompt += "\nMatch spoken names to the closest contact. 'Call Garlon' means 'Andrew Garlon Slezak'.\n\n";
    }

    return prompt;
}

void ClaudeClient::parseResponse(const QJsonObject &json)
{
    // Check for API errors
    if (json.contains("error")) {
        QJsonObject errorObj = json["error"].toObject();
        QString errorMessage = errorObj["message"].toString();
        qWarning() << "ClaudeClient: API error:" << errorObj["type"].toString() << "-" << errorMessage;

        m_isProcessing = false;
        emit processingChanged();
        emit error("API error: " + errorMessage);

        // Clean up history — remove incomplete tool exchanges and the failed message
        sanitizeHistory();
        return;
    }

    if (!json.contains("content")) {
        m_isProcessing = false;
        m_pendingToolCount = 0;
        m_pendingToolResults.clear();
        m_pendingAssistantContent = QJsonArray();
        m_accumulatedText.clear();
        emit processingChanged();
        sanitizeHistory();
        emit error("No content in response");
        return;
    }

    QJsonArray contentArray = json["content"].toArray();
    QString stopReason = json["stop_reason"].toString();

    qDebug() << "ClaudeClient: stop_reason:" << stopReason << "content blocks:" << contentArray.size();

    // Extract text and tool_use blocks
    QString responseText;
    QJsonArray toolUseBlocks;

    for (const QJsonValue &item : contentArray) {
        QJsonObject obj = item.toObject();
        QString type = obj["type"].toString();

        if (type == "text") {
            responseText += obj["text"].toString();
        } else if (type == "tool_use") {
            toolUseBlocks.append(obj);
        }
    }

    // If stop_reason is "tool_use", we need to execute tools and loop
    if (stopReason == "tool_use" && !toolUseBlocks.isEmpty() && m_toolExecutor) {
        qDebug() << "ClaudeClient: Tool use requested —" << toolUseBlocks.size() << "tools";

        // Accumulate any text from this intermediate turn (Claude often speaks + calls tools together)
        if (!responseText.isEmpty()) {
            if (!m_accumulatedText.isEmpty()) m_accumulatedText += " ";
            m_accumulatedText += responseText;
            qDebug() << "ClaudeClient: Accumulated intermediate text:" << responseText.left(100);
        }

        // Add the full assistant message (with tool_use blocks) to history
        addToHistory("assistant", QJsonValue(contentArray));

        // Execute the tools
        executeToolsAndContinue(toolUseBlocks);
        return;
    }

    // Final response (stop_reason == "end_turn" or no tools)
    m_safetyTimer->stop();
    m_isProcessing = false;
    emit processingChanged();

    // Combine accumulated text from tool_use turns with the final response
    QString fullResponse;
    if (!m_accumulatedText.isEmpty()) {
        fullResponse = m_accumulatedText;
        if (!responseText.isEmpty()) {
            fullResponse += " " + responseText;
        }
        m_accumulatedText.clear();
    } else {
        fullResponse = responseText;
    }

    // Add assistant response to history
    if (!m_ephemeralRequest) {
        addToHistory("assistant", fullResponse);
    } else {
        // For ephemeral, remove everything from this request (user msg + any tool exchanges)
        sanitizeHistory();
        // Also remove the original user text message
        if (!m_conversationHistory.isEmpty()) {
            QJsonObject last = m_conversationHistory[m_conversationHistory.size() - 1].toObject();
            if (last["role"].toString() == "user") {
                m_conversationHistory.removeLast();
            }
        }
    }

    m_currentResponse = fullResponse;
    emit responseReceived(fullResponse, QJsonArray());

    setStatusMessage("Response received");
    qDebug() << "ClaudeClient: Final response:" << fullResponse.left(200);
}

// ========================================================================
// TOOL EXECUTION LOOP
// ========================================================================

void ClaudeClient::executeToolsAndContinue(const QJsonArray &toolUseBlocks)
{
    m_pendingToolResults.clear();
    m_pendingToolCount = 0;
    m_pendingAssistantContent = toolUseBlocks; // Store for building tool_result message
    m_activeToolGeneration = m_toolGeneration; // Snapshot generation for stale-completion checks

    // Execute each tool
    for (const QJsonValue &toolVal : toolUseBlocks) {
        QJsonObject toolUse = toolVal.toObject();
        QString toolId = toolUse["id"].toString();
        QString toolName = toolUse["name"].toString();
        QJsonObject input = toolUse["input"].toObject();

        // Guard against malformed tool_use blocks (missing id/name breaks the tool loop)
        if (toolId.isEmpty() || toolName.isEmpty()) {
            qWarning() << "ClaudeClient: Skipping malformed tool_use block — id:" << toolId << "name:" << toolName;
            continue;
        }

        QJsonObject result = m_toolExecutor->executeTool(toolId, toolName, input);

        if (result.isEmpty()) {
            // Async tool — result comes via toolCompleted signal
            m_pendingToolCount++;
            qDebug() << "ClaudeClient: Async tool" << toolName << "pending (id:" << toolId << ")";
        } else {
            // Sync tool — result is immediate
            m_pendingToolResults[toolId] = result;
            qDebug() << "ClaudeClient: Sync tool" << toolName << "completed (id:" << toolId << ")";
        }
    }

    // If all tools were sync, submit results immediately
    if (m_pendingToolCount <= 0) {
        submitToolResults();
    } else {
        // Timeout for async tools — if they don't complete in 10s, submit error results
        // Capture generation so stale timeouts from interrupted requests are discarded
        int gen = m_toolGeneration;
        QTimer::singleShot(10000, this, [this, toolUseBlocks, gen]() {
            if (gen != m_toolGeneration) {
                qDebug() << "ClaudeClient: Ignoring stale tool timeout (generation mismatch)";
                return;
            }
            if (m_pendingToolCount > 0) {
                qWarning() << "ClaudeClient: Async tool timeout —" << m_pendingToolCount << "tools still pending";
                // Fill in error results for any tools that didn't complete
                for (const QJsonValue &toolVal : toolUseBlocks) {
                    QString toolId = toolVal.toObject()["id"].toString();
                    if (!m_pendingToolResults.contains(toolId)) {
                        QJsonObject errorResult;
                        errorResult["status"] = "error";
                        errorResult["error"] = "Tool timed out — service may not be running.";
                        m_pendingToolResults[toolId] = errorResult;
                    }
                }
                m_pendingToolCount = 0;
                submitToolResults();
            }
        });
    }
}

void ClaudeClient::submitToolResults()
{
    qDebug() << "ClaudeClient: Submitting" << m_pendingToolResults.size() << "tool results";

    // Build the user message with tool_result blocks
    QJsonArray toolResultContent;

    for (const QJsonValue &toolVal : m_pendingAssistantContent) {
        QJsonObject toolUse = toolVal.toObject();
        QString toolId = toolUse["id"].toString();

        QJsonObject toolResult;
        toolResult["type"] = "tool_result";
        toolResult["tool_use_id"] = toolId;

        if (m_pendingToolResults.contains(toolId)) {
            QJsonObject result = m_pendingToolResults[toolId];
            toolResult["content"] = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
        } else {
            toolResult["content"] = "Tool execution failed — no result received.";
        }

        toolResultContent.append(toolResult);
    }

    // Add tool_result user message to history
    addToHistory("user", QJsonValue(toolResultContent));

    // Clear pending state
    m_pendingToolResults.clear();
    m_pendingAssistantContent = QJsonArray();
    m_pendingToolCount = 0;

    // Fire another API request — Claude will see the tool results and respond
    fireApiRequest();
}

// ========================================================================
// HELPER METHODS
// ========================================================================

void ClaudeClient::sanitizeHistory()
{
    // Remove trailing messages that are part of an incomplete tool exchange.
    // A valid conversation ends with either:
    //   - assistant(text)  — Claude's final response
    //   - user(text)       — user's message awaiting response
    // Invalid trailing messages (tool_use assistant, tool_result user) are removed.

    while (!m_conversationHistory.isEmpty()) {
        QJsonObject last = m_conversationHistory[m_conversationHistory.size() - 1].toObject();
        QString role = last["role"].toString();
        QJsonValue content = last["content"];

        bool isToolExchange = false;

        if (role == "assistant" && content.isArray()) {
            // Check if this assistant message contains tool_use blocks
            QJsonArray arr = content.toArray();
            for (const QJsonValue &v : arr) {
                if (v.toObject()["type"].toString() == "tool_use") {
                    isToolExchange = true;
                    break;
                }
            }
        } else if (role == "user" && content.isArray()) {
            // Check if this user message contains tool_result blocks
            QJsonArray arr = content.toArray();
            if (!arr.isEmpty() && arr[0].toObject()["type"].toString() == "tool_result") {
                isToolExchange = true;
            }
        }

        if (isToolExchange) {
            qDebug() << "ClaudeClient: sanitizeHistory — removing trailing" << role << "tool exchange message";
            m_conversationHistory.removeLast();
        } else {
            break;
        }
    }

    // Also clean up orphaned messages at the front (same logic as addToHistory)
    while (!m_conversationHistory.isEmpty()) {
        QJsonObject first = m_conversationHistory[0].toObject();
        QString role = first["role"].toString();

        if (role == "assistant") {
            m_conversationHistory.removeFirst();
            continue;
        }
        if (role == "user") {
            QJsonValue content = first["content"];
            if (content.isArray()) {
                QJsonArray arr = content.toArray();
                if (!arr.isEmpty() && arr[0].toObject()["type"].toString() == "tool_result") {
                    m_conversationHistory.removeFirst();
                    continue;
                }
            }
        }
        break;
    }

    qDebug() << "ClaudeClient: sanitizeHistory — history now has" << m_conversationHistory.size() << "messages";
}

void ClaudeClient::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

void ClaudeClient::addToHistory(const QString &role, const QJsonValue &content)
{
    QJsonObject message;
    message["role"] = role;
    message["content"] = content;

    m_conversationHistory.append(message);
    emit conversationActiveChanged();

    // Limit history — keep at most 20 messages, trimming from the front
    // CRITICAL: never leave a tool_result as the first message, and always
    // keep assistant(tool_use) + user(tool_result) pairs together
    while (m_conversationHistory.size() > 20) {
        m_conversationHistory.removeFirst();
    }

    // Ensure the first message in history is a valid starting point:
    // - Must be role=user with text content (not tool_result)
    // - Remove any orphaned assistant(tool_use) or user(tool_result) messages at the front
    while (!m_conversationHistory.isEmpty()) {
        QJsonObject first = m_conversationHistory[0].toObject();
        QString role = first["role"].toString();

        // If it starts with assistant, remove it (orphaned)
        if (role == "assistant") {
            m_conversationHistory.removeFirst();
            continue;
        }

        // If it starts with user, check if the content is tool_result (orphaned)
        if (role == "user") {
            QJsonValue content = first["content"];
            if (content.isArray()) {
                QJsonArray arr = content.toArray();
                if (!arr.isEmpty() && arr[0].toObject()["type"].toString() == "tool_result") {
                    m_conversationHistory.removeFirst();
                    continue;
                }
            }
        }

        break; // First message is a valid user text message
    }

    qDebug() << "ClaudeClient: History now has" << m_conversationHistory.size() << "messages";
}
