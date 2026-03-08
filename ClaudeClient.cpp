#include "ClaudeClient.h"
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
    , m_maxTokens(1024)
    , m_temperature(0.7)
    , m_isConnected(false)
    , m_isProcessing(false)
    , m_statusMessage("Not configured")
{
    qDebug() << "ClaudeClient: Initializing...";

    // Check SSL support
    qDebug() << "ClaudeClient: SSL support:" << QSslSocket::supportsSsl();
    qDebug() << "ClaudeClient: SSL library build version:" << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << "ClaudeClient: SSL library runtime version:" << QSslSocket::sslLibraryVersionString();

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

    // Connect network manager signals
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &ClaudeClient::onNetworkReply);

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

    if (m_isConnected) {
        setStatusMessage("API key configured");
        qDebug() << "ClaudeClient: API key set";
    } else {
        setStatusMessage("API key cleared");
        qDebug() << "ClaudeClient: API key cleared";
    }
}

void ClaudeClient::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        emit modelChanged();
        qDebug() << "ClaudeClient: Model changed to" << m_model;
    }
}

void ClaudeClient::setMaxTokens(int tokens)
{
    tokens = qBound(256, tokens, 4096);
    if (m_maxTokens != tokens) {
        m_maxTokens = tokens;
        emit maxTokensChanged();
        qDebug() << "ClaudeClient: Max tokens set to" << m_maxTokens;
    }
}

void ClaudeClient::setTemperature(double temp)
{
    temp = qBound(0.0, temp, 1.0);
    if (m_temperature != temp) {
        m_temperature = temp;
        emit temperatureChanged();
        qDebug() << "ClaudeClient: Temperature set to" << m_temperature;
    }
}

// ========================================================================
// MESSAGING
// ========================================================================

void ClaudeClient::sendMessage(const QString &message, const QString &systemContext)
{
    if (!m_isConnected || m_apiKey.isEmpty()) {
        emit error("API key not configured");
        setStatusMessage("Error: No API key");
        return;
    }

    if (message.trimmed().isEmpty()) {
        emit error("Empty message");
        return;
    }

    if (m_isProcessing) {
        qWarning() << "ClaudeClient: Already processing a request";
        return;
    }

    m_isProcessing = true;
    emit processingChanged();
    setStatusMessage("Sending request to Claude...");

    // Build system prompt
    QString systemPrompt = systemContext.isEmpty() ? buildSystemPrompt() : systemContext;

    // Build request
    QJsonObject request = buildRequest(message, systemPrompt);

    // Create network request
    QUrl url(API_ENDPOINT);
    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("x-api-key", m_apiKey.toUtf8());
    networkRequest.setRawHeader("anthropic-version", API_VERSION);

    // Enable HTTP/2
    networkRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    // Send request
    QByteArray requestData = QJsonDocument(request).toJson(QJsonDocument::Compact);
    qDebug() << "ClaudeClient: Sending request:" << requestData.left(200) << "...";

    m_currentReply = m_networkManager->post(networkRequest, requestData);

    // Connect reply signals
    connect(m_currentReply, &QNetworkReply::readyRead,
            this, &ClaudeClient::onReadyRead);
    connect(m_currentReply, &QNetworkReply::errorOccurred,
            this, &ClaudeClient::onNetworkError);

    // Add user message to history
    addToHistory("user", message);
}

void ClaudeClient::sendMessageWithContext(const QString &message)
{
    sendMessage(message, buildSystemPrompt());
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

    m_isProcessing = false;
    emit processingChanged();
    setStatusMessage("Request cancelled");
}

// ========================================================================
// SYSTEM CONTEXT
// ========================================================================

void ClaudeClient::updateSystemContext(const QJsonObject &context)
{
    m_systemContext = context;
    qDebug() << "ClaudeClient: System context updated with" << context.keys().size() << "fields";
}

void ClaudeClient::setAvailableTools(const QJsonArray &tools)
{
    m_availableTools = tools;
    qDebug() << "ClaudeClient: Available tools set:" << tools.size() << "tools";
}

void ClaudeClient::setContactNames(const QStringList &names)
{
    m_contactNames = names;
    qDebug() << "ClaudeClient: Contact names set:" << names.size() << "contacts";
}

// ========================================================================
// NETWORK HANDLING
// ========================================================================

void ClaudeClient::onNetworkReply(QNetworkReply *reply)
{
    if (reply != m_currentReply) {
        return;
    }

    m_isProcessing = false;
    emit processingChanged();

    // Debug HTTP response details
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Use stream buffer (data already read by onReadyRead)
    QByteArray responseData = m_streamBuffer.toUtf8();
    m_streamBuffer.clear();

    qDebug() << "ClaudeClient: HTTP Status Code:" << statusCode;
    qDebug() << "ClaudeClient: Response size:" << responseData.size() << "bytes";
    qDebug() << "ClaudeClient: Response data:" << responseData.left(500);
    qDebug() << "ClaudeClient: Network error code:" << reply->error();

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        qWarning() << "ClaudeClient: Network error:" << errorMsg;
        qWarning() << "ClaudeClient: HTTP Status:" << statusCode;
        qWarning() << "ClaudeClient: Response body:" << responseData;
        emit error("Network error: " + errorMsg);
        setStatusMessage("Error: " + errorMsg);
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    // Read complete response
    qDebug() << "ClaudeClient: Received response:" << responseData.left(200) << "...";

    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        emit error("Invalid JSON response");
        setStatusMessage("Error: Invalid response");
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    parseResponse(doc.object());

    reply->deleteLater();
    m_currentReply = nullptr;
}

void ClaudeClient::onReadyRead()
{
    if (!m_currentReply) {
        return;
    }

    // For streaming responses (if we enable streaming in the future)
    QByteArray chunk = m_currentReply->readAll();
    m_streamBuffer.append(QString::fromUtf8(chunk));

    // Note: Claude API doesn't support streaming in the same way as some other APIs
    // This is here for potential future implementation
}

void ClaudeClient::onNetworkError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);

    if (!m_currentReply) {
        return;
    }

    QString errorMsg = m_currentReply->errorString();
    qWarning() << "ClaudeClient: Network error:" << errorMsg;

    m_isProcessing = false;
    emit processingChanged();
    emit this->error("Connection error: " + errorMsg);
    setStatusMessage("Error: " + errorMsg);
}

// ========================================================================
// REQUEST/RESPONSE BUILDING
// ========================================================================

QJsonObject ClaudeClient::buildRequest(const QString &userMessage, const QString &systemPrompt)
{
    QJsonObject request;

    // Model
    request["model"] = m_model;

    // Max tokens
    request["max_tokens"] = m_maxTokens;

    // Temperature
    request["temperature"] = m_temperature;

    // System prompt (with context)
    request["system"] = systemPrompt;

    // Messages array (conversation history + new message)
    QJsonArray messages = m_conversationHistory;

    // Add current user message
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);

    request["messages"] = messages;

    // Tools (if any)
    if (!m_availableTools.isEmpty()) {
        request["tools"] = m_availableTools;
    }

    return request;
}

QString ClaudeClient::buildSystemPrompt() const
{
    QString prompt = "You are Sammy, a friendly and helpful AI assistant integrated into a vehicle's head unit system. ";
    prompt += "You have a warm, professional female voice and personality. ";
    prompt += "Your role is to help the driver with navigation, phone calls, messaging, music control, ";
    prompt += "and general queries while prioritizing safety.\n\n";

    prompt += "IMPORTANT SAFETY RULES:\n";
    prompt += "- Always prioritize driver safety and minimize distraction\n";
    prompt += "- Keep responses concise and clear for voice interaction\n";
    prompt += "- Confirm potentially dangerous actions (like calling while driving)\n";
    prompt += "- Never engage in lengthy conversations while the vehicle is moving\n\n";

    // Add system context if available
    if (!m_systemContext.isEmpty()) {
        prompt += "CURRENT SYSTEM STATUS:\n";

        if (m_systemContext.contains("speed")) {
            prompt += QString("- Vehicle speed: %1 mph\n").arg(m_systemContext["speed"].toInt());
        }
        if (m_systemContext.contains("fuel")) {
            prompt += QString("- Fuel level: %1%\n").arg(m_systemContext["fuel"].toInt());
        }
        if (m_systemContext.contains("location")) {
            prompt += QString("- Current location: %1\n").arg(m_systemContext["location"].toString());
        }
        if (m_systemContext.contains("time")) {
            prompt += QString("- Current time: %1\n").arg(m_systemContext["time"].toString());
        }
        if (m_systemContext.contains("weather")) {
            prompt += QString("- Weather: %1\n").arg(m_systemContext["weather"].toString());
        }
        if (m_systemContext.contains("destination")) {
            prompt += QString("- Navigation destination: %1\n").arg(m_systemContext["destination"].toString());
        }
        if (m_systemContext.contains("eta")) {
            prompt += QString("- ETA: %1\n").arg(m_systemContext["eta"].toString());
        }

        prompt += "\n";
    }

    prompt += "AVAILABLE ACTIONS:\n";
    prompt += "- Make phone calls to contacts\n";
    prompt += "- Send messages (with voice dictation)\n";
    prompt += "- Control music playback (play, pause, skip, search)\n";
    prompt += "- Navigate to destinations\n";
    prompt += "- Read incoming messages and notifications\n";
    prompt += "- Answer questions and provide information\n\n";

    prompt += "VOICE COMMAND EXECUTION:\n";
    prompt += "When the user requests a phone action (call, text, read messages), respond with BOTH:\n";
    prompt += "1. A natural language response acknowledging the request\n";
    prompt += "2. A JSON command block in this exact format:\n\n";
    prompt += "```json\n";
    prompt += "{\n";
    prompt += "  \"action\": \"call|message|read_messages\",\n";
    prompt += "  \"contact_name\": \"Contact Name\",\n";
    prompt += "  \"message_body\": \"Message text here\",\n";
    prompt += "  \"phone_number\": \"+1234567890\"\n";
    prompt += "}\n";
    prompt += "```\n\n";
    prompt += "CRITICAL: For contact_name, use EXACTLY what the user said - do not try to guess or expand the name.\n";
    prompt += "The system will search the user's contacts for matches. Examples:\n";
    prompt += "- If user says 'call Garlon', use contact_name: 'Garlon' (NOT 'Paul Garland' or any guess)\n";
    prompt += "- If user says 'call Ari', use contact_name: 'Ari' (the system will find contacts with Ari in the name)\n";
    prompt += "- If user says 'text mom', use contact_name: 'mom'\n\n";
    prompt += "Examples:\n";
    prompt += "- User: \"Call Mom\" → Response: \"Sure, I'll call Mom for you.\" + {\"action\": \"call\", \"contact_name\": \"Mom\"}\n";
    prompt += "- User: \"Text John I'm running late\" → Response: \"I'll send that message to John.\" + {\"action\": \"message\", \"contact_name\": \"John\", \"message_body\": \"I'm running late\"}\n";
    prompt += "- User: \"Call Garlon\" → Response: \"I'll call Garlon for you.\" + {\"action\": \"call\", \"contact_name\": \"Garlon\"}\n\n";

    prompt += "When the user requests an action, clearly state what you're about to do and ask for confirmation if it's safety-critical. ";
    prompt += "Keep all responses brief and natural for voice conversation.\n\n";

    // Add contact names if available
    if (!m_contactNames.isEmpty()) {
        prompt += "USER'S CONTACTS (for matching spoken names):\n";
        // Include all contacts - this helps Claude understand partial matches
        // The list is comma-separated to minimize token usage
        prompt += m_contactNames.join(", ");
        prompt += "\n\nWhen the user mentions a name, use your knowledge of these contacts to find the best match. ";
        prompt += "For example, if user says 'call Garlon' and there's a contact 'Andrew Garlon Slezak', use 'Andrew Garlon Slezak' as contact_name.\n";
    }

    return prompt;
}

void ClaudeClient::parseResponse(const QJsonObject &json)
{
    // Check for API errors
    if (json.contains("error")) {
        QJsonObject errorObj = json["error"].toObject();
        QString errorType = errorObj["type"].toString();
        QString errorMessage = errorObj["message"].toString();

        qWarning() << "ClaudeClient: API error:" << errorType << "-" << errorMessage;
        emit error("API error: " + errorMessage);
        setStatusMessage("Error: " + errorMessage);
        return;
    }

    // Extract response content
    if (!json.contains("content")) {
        emit error("No content in response");
        setStatusMessage("Error: Empty response");
        return;
    }

    QJsonArray contentArray = json["content"].toArray();
    QString responseText;
    QJsonArray toolCalls;

    for (const QJsonValue &contentItem : contentArray) {
        QJsonObject contentObj = contentItem.toObject();
        QString type = contentObj["type"].toString();

        if (type == "text") {
            responseText += contentObj["text"].toString();
        } else if (type == "tool_use") {
            toolCalls.append(contentObj);

            // Emit individual tool call
            QString toolName = contentObj["name"].toString();
            QJsonObject toolParams = contentObj["input"].toObject();
            emit toolCallRequested(toolName, toolParams);
        }
    }

    // Add assistant response to history
    addToHistory("assistant", responseText);

    // Emit complete response
    m_currentResponse = responseText;
    emit responseReceived(responseText, toolCalls);

    setStatusMessage("Response received");
    qDebug() << "ClaudeClient: Response:" << responseText;
}

void ClaudeClient::parseStreamChunk(const QString &chunk)
{
    // For future streaming implementation
    Q_UNUSED(chunk);
}

// ========================================================================
// HELPER METHODS
// ========================================================================

void ClaudeClient::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "ClaudeClient:" << msg;
    }
}

void ClaudeClient::addToHistory(const QString &role, const QString &content)
{
    QJsonObject message;
    message["role"] = role;
    message["content"] = content;

    m_conversationHistory.append(message);
    emit conversationActiveChanged();

    // Limit history to last 10 messages (5 exchanges)
    while (m_conversationHistory.size() > 10) {
        m_conversationHistory.removeFirst();
    }

    qDebug() << "ClaudeClient: Added to history:" << role << "-" << content.left(50) << "...";
}
