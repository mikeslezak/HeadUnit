#ifndef VOICECOMMANDHANDLER_H
#define VOICECOMMANDHANDLER_H

#include <QObject>
#include <QString>
#include <QVariantMap>

/**
 * VoiceCommandHandler - Parses Claude AI responses and executes phone commands
 *
 * This class integrates with Claude AI to parse voice commands and execute actions like:
 * - Making phone calls
 * - Sending text messages
 * - Reading messages
 * - Controlling media playback
 *
 * Command Flow:
 * 1. User speaks command (via microphone)
 * 2. Speech-to-text converts to text
 * 3. Claude AI interprets the command
 * 4. VoiceCommandHandler parses Claude's response
 * 5. Executes the appropriate action (call, text, etc.)
 * 6. Uses TTS to provide feedback to user
 */
class VoiceCommandHandler : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool awaitingConfirmation READ awaitingConfirmation NOTIFY awaitingConfirmationChanged)
    Q_PROPERTY(QString pendingAction READ pendingAction NOTIFY pendingActionChanged)

public:
    explicit VoiceCommandHandler(QObject *parent = nullptr);

    // Property getters
    QString statusMessage() const { return m_statusMessage; }
    bool awaitingConfirmation() const { return m_awaitingConfirmation; }
    QString pendingAction() const { return m_pendingAction; }

public slots:
    /**
     * Process Claude's response to extract and execute commands
     * @param claudeResponse: The full response text from Claude AI
     */
    void processClaudeResponse(const QString &claudeResponse);

    /**
     * Confirm pending action (for SMS confirmation flow)
     */
    void confirmAction();

    /**
     * Cancel pending action
     */
    void cancelAction();

    /**
     * Set dependencies - must be called before use
     */
    void setContactManager(QObject *contactManager);
    void setMessageManager(QObject *messageManager);
    void setBluetoothManager(QObject *bluetoothManager);
    void setGoogleTTS(QObject *googleTTS);

signals:
    void statusMessageChanged();
    void awaitingConfirmationChanged();
    void pendingActionChanged();

    /**
     * Emitted when a command is successfully executed
     */
    void commandExecuted(const QString &action, const QString &details);

    /**
     * Emitted when command execution fails
     */
    void commandFailed(const QString &action, const QString &error);

    /**
     * Request user confirmation for an action
     */
    void confirmationRequested(const QString &action, const QString &details);

    /**
     * Emitted when a navigation command is recognized
     * @param destination: The place or address to navigate to
     */
    void navigationRequested(const QString &destination);

    /**
     * Emitted when a places search is requested
     * @param query: Search query (e.g. "tacos")
     * @param category: Optional category filter
     */
    void placesSearchRequested(const QString &query, const QString &category);

    /**
     * Emitted when Claude's response includes expects_reply: true
     */
    void followUpExpected();

    /**
     * Emitted when quiet mode should be toggled
     * @param enabled: true to silence proactive alerts
     */
    void quietModeRequested(bool enabled);

private:
    void setStatusMessage(const QString &msg);
    void setAwaitingConfirmation(bool awaiting);
    void setPendingAction(const QString &action);

    /**
     * Parse JSON-formatted command from Claude's response
     * Expected format:
     * {
     *   "action": "call|message|read_messages",
     *   "contact_name": "John Doe",
     *   "message_body": "I'm running late",
     *   "phone_number": "+1234567890"
     * }
     */
    QVariantMap parseCommandJSON(const QString &claudeResponse);

    /**
     * Execute specific command types
     */
    void executeCallCommand(const QVariantMap &command);
    void executeMessageCommand(const QVariantMap &command);
    void executeReadMessagesCommand(const QVariantMap &command);
    void executeNavigateCommand(const QVariantMap &command);
    void executeSearchPlacesCommand(const QVariantMap &command);
    void executeQuietModeCommand(const QVariantMap &command);

    /**
     * Helper methods
     */
    QString findContactPhoneNumber(const QString &contactName);
    void speakFeedback(const QString &text);

    // Member variables
    QString m_statusMessage;
    bool m_awaitingConfirmation;
    QString m_pendingAction;

    // Pending command data (for confirmation flow)
    QVariantMap m_pendingCommand;

    // Dependencies (set via setters)
    QObject *m_contactManager;
    QObject *m_messageManager;
    QObject *m_bluetoothManager;
    QObject *m_googleTTS;
};

#endif // VOICECOMMANDHANDLER_H
