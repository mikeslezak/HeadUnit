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

    // Conversation inactivity timeout — clear history after 60s of no messages
    m_conversationTimer = new QTimer(this);
    m_conversationTimer->setSingleShot(true);
    m_conversationTimer->setInterval(60 * 1000);
    connect(m_conversationTimer, &QTimer::timeout, this, [this]() {
        if (!m_conversationHistory.isEmpty()) {
            qDebug() << "ClaudeClient: Conversation timed out, clearing history";
            clearConversation();
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
        emit error("Voice assistant is busy, try again in a moment");
        return;
    }

    m_isProcessing = true;
    emit processingChanged();
    setStatusMessage("Sending request to Claude...");

    // Reset conversation inactivity timer
    m_conversationTimer->start();

    // Build system prompt — always use buildSystemPrompt, inject live context if provided
    QString systemPrompt = buildSystemPrompt();
    if (!systemContext.isEmpty()) {
        systemPrompt += "\nLIVE CONTEXT:\n" + systemContext;
    }

    // Build request
    QJsonObject request = buildRequest(message, systemPrompt);

    // Create network request
    QUrl url(API_ENDPOINT);
    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("x-api-key", m_apiKey.toUtf8());
    networkRequest.setRawHeader("anthropic-version", API_VERSION);
    networkRequest.setTransferTimeout(30000);

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

    // Null out so the finished handler (onNetworkReply) skips re-processing
    m_currentReply = nullptr;

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
    QString prompt;

    // Identity & personality
    prompt += "You are Jarvis, an AI copilot riding shotgun in a truck. ";
    prompt += "You have full situational awareness: GPS, weather, vehicle telemetry, and active route data. ";
    prompt += "You're laid back, witty, and talk like a buddy on a road trip — not a corporate assistant. ";
    prompt += "Think casual, warm, a little playful. You can crack a joke or throw in some personality. ";
    prompt += "But you're also sharp and helpful — you keep it brief and don't ramble.\n\n";

    // Voice output rules
    prompt += "VOICE OUTPUT RULES:\n";
    prompt += "- Responses are spoken aloud via TTS. Keep them SHORT: 1-3 sentences max.\n";
    prompt += "- NEVER use markdown (no **, ##, bullets, numbered lists) or emojis.\n";
    prompt += "- Write exactly how a chill person would actually talk. Use contractions, casual phrasing.\n";
    prompt += "- Avoid stiff or formal language. Say \"yeah\" not \"certainly\", \"gotcha\" not \"understood\".\n";
    prompt += "- Use the live context to give informed, location-aware answers.\n";
    prompt += "- If asked about location, weather, or route, reference the actual data.\n\n";

    // Actions
    prompt += "AVAILABLE ACTIONS:\n";
    prompt += "When the user requests an action, respond with a brief spoken acknowledgment AND a JSON command block:\n\n";
    prompt += "```json\n";
    prompt += "{\n";
    prompt += "  \"action\": \"call|message|read_messages|navigate|search_places\",\n";
    prompt += "  \"contact_name\": \"Contact Name\",\n";
    prompt += "  \"message_body\": \"Message text\",\n";
    prompt += "  \"phone_number\": \"+1234567890\",\n";
    prompt += "  \"destination\": \"Place or address\",\n";
    prompt += "  \"query\": \"search query for places\",\n";
    prompt += "  \"category\": \"restaurant|gas_station|hotel|etc\",\n";
    prompt += "  \"expects_reply\": true\n";
    prompt += "}\n";
    prompt += "```\n\n";

    prompt += "ACTION RULES:\n";
    prompt += "- navigate: ALWAYS emit this action when the user wants to go somewhere, even if GPS is unavailable. Set destination to what the user said. The system will geocode the text. Examples: 'Starbucks', '123 Main St', 'GoodLife Fitness in Okotoks'.\n";
    prompt += "- call/message: Use contact_name EXACTLY as the user said.\n";
    prompt += "- search_places: Use when user wants to find nearby places (food, gas, hotels, etc). Set query to the search term and category if obvious.\n";
    prompt += "- quiet_mode: Use when user says 'stop alerts', 'quiet mode', 'be quiet', or similar. Set enabled to true to silence proactive alerts, false to re-enable.\n";
    prompt += "- expects_reply: Set to true when you ask a question (\"Want directions?\", \"Which one?\"). This keeps the mic open for a follow-up without the wake word.\n\n";

    prompt += "EXAMPLES:\n";
    prompt += "- \"Navigate to Starbucks\" -> \"Navigating to Starbucks.\" + {\"action\": \"navigate\", \"destination\": \"Starbucks\"}\n";
    prompt += "- \"Find tacos nearby\" -> \"Searching for tacos near you.\" + {\"action\": \"search_places\", \"query\": \"tacos\", \"category\": \"restaurant\"}\n";
    prompt += "- \"Call Mom\" -> \"Calling Mom.\" + {\"action\": \"call\", \"contact_name\": \"Mom\"}\n";
    prompt += "- \"Where am I?\" -> Reference GPS coordinates and location name from context. No JSON needed.\n";
    prompt += "- \"What's the weather ahead?\" -> Reference route weather data from context. No JSON needed.\n\n";

    prompt += "MULTI-TURN CONVERSATIONS:\n";
    prompt += "- When presenting search results or options, set expects_reply to true.\n";
    prompt += "- After presenting places, if user says a number or name, navigate to that choice.\n";
    prompt += "- If the user says 'yes' after you ask about directions, emit a navigate action.\n";
    prompt += "- If user says 'goodbye', 'thanks', 'that's all', or 'never mind', just respond briefly. No action needed.\n\n";

    // Contact names
    if (!m_contactNames.isEmpty()) {
        prompt += "USER'S CONTACTS:\n";
        prompt += m_contactNames.join(", ");
        prompt += "\nMatch spoken names to the closest contact. E.g., 'call Garlon' -> 'Andrew Garlon Slezak'.\n\n";
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
    // Remove in pairs to maintain user/assistant ordering
    while (m_conversationHistory.size() > 10) {
        m_conversationHistory.removeFirst();
        if (!m_conversationHistory.isEmpty())
            m_conversationHistory.removeFirst();
    }

    qDebug() << "ClaudeClient: Added to history:" << role << "-" << content.left(50) << "...";
}
