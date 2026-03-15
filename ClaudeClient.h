#ifndef CLAUDECLIENT_H
#define CLAUDECLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMap>

class ToolExecutor;

/**
 * ClaudeClient - Claude API Integration for Jarvis Voice Assistant
 *
 * Handles communication with Anthropic's Claude API including native tool use.
 * When Claude returns tool_use blocks, ClaudeClient executes them via ToolExecutor,
 * submits tool_result messages back, and loops until Claude gives a final text response.
 */
class ClaudeClient : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int maxTokens READ maxTokens WRITE setMaxTokens NOTIFY maxTokensChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool conversationActive READ conversationActive NOTIFY conversationActiveChanged)

public:
    explicit ClaudeClient(QObject *parent = nullptr);
    ~ClaudeClient();

    bool isConnected() const { return m_isConnected; }
    bool isProcessing() const { return m_isProcessing; }
    QString model() const { return m_model; }
    int maxTokens() const { return m_maxTokens; }
    double temperature() const { return m_temperature; }
    QString statusMessage() const { return m_statusMessage; }
    bool conversationActive() const { return !m_conversationHistory.isEmpty(); }

public slots:
    void setApiKey(const QString &apiKey);
    void setModel(const QString &model);
    void setMaxTokens(int tokens);
    void setTemperature(double temp);

    void sendMessage(const QString &message, const QString &systemContext = QString(), bool ephemeral = false);
    void clearConversation();
    void cancelRequest();

    void setAvailableTools(const QJsonArray &tools);
    void setToolExecutor(ToolExecutor *executor);
    void setContactNames(const QStringList &names);

signals:
    void connectionChanged();
    void processingChanged();
    void modelChanged();
    void maxTokensChanged();
    void temperatureChanged();
    void statusMessageChanged();
    void conversationActiveChanged();

    /**
     * Emitted when Claude's final text response is ready (after all tools have been executed).
     * toolCalls is empty — tools are handled internally now.
     */
    void responseReceived(const QString &response, const QJsonArray &toolCalls);

    void error(const QString &message);

private slots:
    void onNetworkReply(QNetworkReply *reply);
    void onReadyRead();

private:
    // Build API request from current conversation state
    QJsonObject buildRequest(const QString &systemPrompt);

    // Build system prompt
    QString buildSystemPrompt() const;

    // Parse API response — may trigger tool loop or emit final response
    void parseResponse(const QJsonObject &json);

    // Execute tool_use blocks from Claude's response, then submit results
    void executeToolsAndContinue(const QJsonArray &contentArray);

    // Submit tool results back to Claude (another API call)
    void submitToolResults();

    // Fire an API request from current m_conversationHistory
    void fireApiRequest();

    void setStatusMessage(const QString &msg);

    // Remove incomplete tool exchanges from history (trailing tool_use/tool_result pairs)
    void sanitizeHistory();

    // Add structured message to history (supports content arrays for tool use)
    void addToHistory(const QString &role, const QJsonValue &content);

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

    // Tool use loop
    ToolExecutor *m_toolExecutor = nullptr;
    QJsonArray m_pendingAssistantContent; // Assistant's content array with tool_use blocks
    QMap<QString, QJsonObject> m_pendingToolResults; // tool_use_id -> result
    int m_pendingToolCount = 0; // Outstanding async tools
    int m_toolGeneration = 0;  // Incremented on cancel/new request — stale singleShots check this
    int m_activeToolGeneration = 0; // Snapshot of m_toolGeneration when current tools were dispatched
    QString m_currentSystemPrompt; // Cached for tool loop re-requests
    QString m_accumulatedText; // Text from intermediate tool_use turns (spoken after final response)

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
