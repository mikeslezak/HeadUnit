#ifndef CLAUDECLIENT_H
#define CLAUDECLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

/**
 * ClaudeClient - Claude API Integration for Jarvis Voice Assistant
 *
 * This class handles all communication with Anthropic's Claude API for
 * natural language understanding and intelligent responses.
 *
 * Features:
 * - Async streaming responses from Claude API
 * - Conversation history management
 * - System context injection (vehicle status, navigation, etc.)
 * - Tool/function calling support for actions
 * - Secure API key management
 * - Error handling
 * - Offline mode graceful degradation
 *
 * Usage:
 *   ClaudeClient *client = new ClaudeClient(this);
 *   client->setApiKey("sk-ant-...");
 *   connect(client, &ClaudeClient::responseReceived, ...);
 *   client->sendMessage("Navigate to the nearest gas station");
 */
class ClaudeClient : public QObject
{
    Q_OBJECT

    // ========== PROPERTIES ==========

    /**
     * Connection Status
     * True when API is configured and reachable
     */
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)

    /**
     * Processing State
     * True when waiting for API response
     */
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)

    /**
     * Model Name
     * Claude model to use (e.g., "claude-3-5-sonnet-20241022")
     */
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)

    /**
     * Max Tokens
     * Maximum tokens in response (default: 1024)
     */
    Q_PROPERTY(int maxTokens READ maxTokens WRITE setMaxTokens NOTIFY maxTokensChanged)

    /**
     * Temperature
     * Randomness in responses: 0.0 (deterministic) to 1.0 (creative)
     */
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)

    /**
     * Status Message
     * Current status for debugging/display
     */
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    /**
     * Conversation Active
     * True if there's an active conversation with history
     */
    Q_PROPERTY(bool conversationActive READ conversationActive NOTIFY conversationActiveChanged)

public:
    explicit ClaudeClient(QObject *parent = nullptr);
    ~ClaudeClient();

    // ========== PROPERTY GETTERS ==========

    bool isConnected() const { return m_isConnected; }
    bool isProcessing() const { return m_isProcessing; }
    QString model() const { return m_model; }
    int maxTokens() const { return m_maxTokens; }
    double temperature() const { return m_temperature; }
    QString statusMessage() const { return m_statusMessage; }
    bool conversationActive() const { return !m_conversationHistory.isEmpty(); }

public slots:
    // ========== CONFIGURATION ==========

    /**
     * Set Claude API key
     * Get from: https://console.anthropic.com/
     * @param apiKey: Your API key (sk-ant-...)
     */
    void setApiKey(const QString &apiKey);

    /**
     * Set Claude model to use
     * @param model: Model ID (e.g., "claude-3-5-sonnet-20241022")
     */
    void setModel(const QString &model);

    /**
     * Set maximum response tokens
     * @param tokens: Max tokens (256-4096 recommended)
     */
    void setMaxTokens(int tokens);

    /**
     * Set response temperature
     * @param temp: 0.0 (deterministic) to 1.0 (creative)
     */
    void setTemperature(double temp);

    // ========== MESSAGING ==========

    /**
     * Send message to Claude
     * @param message: User's spoken/typed message
     * @param systemContext: Optional system context (JSON string with vehicle state, etc.)
     */
    void sendMessage(const QString &message, const QString &systemContext = QString(), bool ephemeral = false);

    /**
     * Clear conversation history
     * Starts a fresh conversation
     */
    void clearConversation();

    /**
     * Cancel current request
     */
    void cancelRequest();

    // ========== SYSTEM CONTEXT ==========

    /**
     * Set available tools/functions Claude can call
     * @param tools: JSON array of tool definitions
     */
    void setAvailableTools(const QJsonArray &tools);

    /**
     * Set contact names for context (helps Claude understand who user wants to contact)
     * @param names: List of contact names from phone
     */
    void setContactNames(const QStringList &names);

signals:
    // ========== SIGNALS ==========

    void connectionChanged();
    void processingChanged();
    void modelChanged();
    void maxTokensChanged();
    void temperatureChanged();
    void statusMessageChanged();
    void conversationActiveChanged();

    /**
     * Emitted when Claude responds (complete message)
     * @param response: Claude's text response
     * @param toolCalls: Any tool/function calls requested (JSON array)
     */
    void responseReceived(const QString &response, const QJsonArray &toolCalls);

    /**
     * Emitted on errors
     * @param message: Error description
     */
    void error(const QString &message);

private slots:
    /**
     * Handle network response
     */
    void onNetworkReply(QNetworkReply *reply);

    /**
     * Handle streaming data
     */
    void onReadyRead();

private:
    // ========== HELPER METHODS ==========

    /**
     * Build API request JSON
     * @param userMessage: User's message
     * @param systemPrompt: System prompt with context
     * @return: Request JSON object
     */
    QJsonObject buildRequest(const QString &userMessage, const QString &systemPrompt);

    /**
     * Build system prompt with context
     * @return: Complete system prompt string
     */
    QString buildSystemPrompt() const;

    /**
     * Parse API response
     * @param json: Response JSON object
     */
    void parseResponse(const QJsonObject &json);

    /**
     * Set status message and emit signal
     */
    void setStatusMessage(const QString &msg);

    /**
     * Add message to conversation history
     */
    void addToHistory(const QString &role, const QString &content);

    // ========== MEMBER VARIABLES ==========

    // Network
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;

    // Configuration
    QString m_apiKey;
    QString m_model;
    int m_maxTokens;
    double m_temperature;

    // State
    bool m_isConnected;
    bool m_isProcessing;
    QString m_statusMessage;

    // Conversation
    QJsonArray m_conversationHistory;
    QJsonArray m_availableTools;
    QStringList m_contactNames;

    // Streaming
    QByteArray m_streamBuffer;
    QString m_currentResponse;

    // Pending user message (added to history only on successful response)
    QString m_pendingUserMessage;
    bool m_ephemeralRequest = false;

    // Conversation inactivity timeout
    QTimer *m_conversationTimer;

    // Safety timeout for m_isProcessing
    QTimer *m_safetyTimer;

    // Constants
    static constexpr const char* API_ENDPOINT = "https://api.anthropic.com/v1/messages";
    static constexpr const char* API_VERSION = "2023-06-01";
    static constexpr const char* DEFAULT_MODEL = "claude-haiku-4-5-20251001";
};

#endif // CLAUDECLIENT_H
