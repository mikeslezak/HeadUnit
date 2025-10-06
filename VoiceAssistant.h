#ifndef VOICEASSISTANT_H
#define VOICEASSISTANT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

// TextToSpeech is optional - only include if available
#if __has_include(<QtTextToSpeech/QTextToSpeech>)
#include <QtTextToSpeech/QTextToSpeech>
#define HAS_TEXT_TO_SPEECH 1
#else
#define HAS_TEXT_TO_SPEECH 0
// Forward declare to avoid errors
class QTextToSpeech {};
#endif

// Platform-specific includes
#ifndef Q_OS_WIN
#include <QBluetoothAddress>
#include <QBluetoothSocket>
#include <QBluetoothServiceInfo>
#endif

/**
 * VoiceAssistant - Handles Voice Control Features
 *
 * This class provides:
 * - Activation of phone's voice assistant (Siri/Google Assistant)
 * - Text-to-speech for reading messages aloud (if available)
 * - Voice command recognition via phone
 * - Quick reply templates for messages
 *
 * Features:
 * - Siri activation (iPhone)
 * - Google Assistant activation (Android)
 * - Auto-read incoming messages while driving
 * - Announce caller names
 * - Voice-to-text for message dictation
 * - Customizable quick replies
 */
class VoiceAssistant : public QObject
{
    Q_OBJECT

    // ========== PROPERTIES ==========

    /**
     * Connection Status
     * True when connected to phone's voice services
     */
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)

    /**
     * Listening State
     * True when voice assistant is actively listening
     */
    Q_PROPERTY(bool isListening READ isListening NOTIFY listeningChanged)

    /**
     * Voice Active
     * True when text-to-speech is currently speaking
     */
    Q_PROPERTY(bool isVoiceActive READ isVoiceActive NOTIFY voiceActiveChanged)

    /**
     * Auto-Read Messages
     * If true, incoming messages are read aloud automatically
     */
    Q_PROPERTY(bool autoReadMessages READ autoReadMessages WRITE setAutoReadMessages NOTIFY autoReadMessagesChanged)

    /**
     * Voice Volume
     * Range: 0-100
     */
    Q_PROPERTY(int voiceVolume READ voiceVolume WRITE setVoiceVolume NOTIFY voiceVolumeChanged)

    /**
     * Speech Rate
     * -1.0 (slow) to 1.0 (fast), 0.0 = normal
     */
    Q_PROPERTY(double speechRate READ speechRate WRITE setSpeechRate NOTIFY speechRateChanged)

    /**
     * Active Assistant
     * "Siri", "Google Assistant", "Alexa", or "none"
     */
    Q_PROPERTY(QString activeAssistant READ activeAssistant NOTIFY activeAssistantChanged)

    /**
     * Quick Replies
     * Pre-defined text responses for quick messaging
     */
    Q_PROPERTY(QStringList quickReplies READ quickReplies NOTIFY quickRepliesChanged)

    /**
     * Status Message
     * Current status for debugging/display
     */
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    /**
     * Has Text-To-Speech
     * True if TextToSpeech module is available
     */
    Q_PROPERTY(bool hasTextToSpeech READ hasTextToSpeech CONSTANT)

public:
    explicit VoiceAssistant(QObject *parent = nullptr);
    ~VoiceAssistant();

    // ========== PROPERTY GETTERS ==========

    bool isConnected() const { return m_isConnected; }
    bool isListening() const { return m_isListening; }
    bool isVoiceActive() const { return m_isVoiceActive; }
    bool autoReadMessages() const { return m_autoReadMessages; }
    int voiceVolume() const { return m_voiceVolume; }
    double speechRate() const { return m_speechRate; }
    QString activeAssistant() const { return m_activeAssistant; }
    QStringList quickReplies() const { return m_quickReplies; }
    QString statusMessage() const { return m_statusMessage; }
    bool hasTextToSpeech() const { return HAS_TEXT_TO_SPEECH; }

public slots:
    // ========== CONNECTION MANAGEMENT ==========

    /**
     * Connect to phone's voice services
     * @param deviceAddress: Bluetooth address of phone
     */
    void connectToPhone(const QString &deviceAddress);

    /**
     * Disconnect from voice services
     */
    void disconnect();

    // ========== VOICE ASSISTANT ==========

    /**
     * Activate phone's voice assistant
     * Triggers Siri (iOS) or Google Assistant (Android)
     */
    void activateAssistant();

    /**
     * Deactivate voice assistant
     */
    void deactivateAssistant();

    // ========== TEXT-TO-SPEECH ==========

    /**
     * Speak text aloud
     * Uses local TTS engine
     * @param text: Text to speak
     */
    void speak(const QString &text);

    /**
     * Stop current speech
     */
    void stopSpeaking();

    /**
     * Read message aloud
     * @param sender: Sender name
     * @param message: Message text
     */
    void readMessage(const QString &sender, const QString &message);

    /**
     * Announce incoming caller
     * @param callerName: Name of caller
     */
    void announceCaller(const QString &callerName);

    // ========== SETTINGS ==========

    /**
     * Enable/disable auto-read messages
     * @param enabled: True to auto-read
     */
    void setAutoReadMessages(bool enabled);

    /**
     * Set voice volume
     * @param volume: 0-100
     */
    void setVoiceVolume(int volume);

    /**
     * Set speech rate
     * @param rate: -1.0 (slow) to 1.0 (fast)
     */
    void setSpeechRate(double rate);

    // ========== QUICK REPLIES ==========

    /**
     * Send quick reply by index
     * @param index: Index in quickReplies list
     */
    void sendQuickReply(int index);

    /**
     * Add custom quick reply
     * @param text: Reply text
     */
    void addQuickReply(const QString &text);

    /**
     * Remove quick reply
     * @param index: Index in quickReplies list
     */
    void removeQuickReply(int index);

    // ========== VOICE COMMANDS ==========

    /**
     * Process recognized voice command
     * @param command: Recognized text from voice assistant
     */
    void processVoiceCommand(const QString &command);

signals:
    // ========== SIGNALS ==========

    void connectionChanged();
    void listeningChanged();
    void voiceActiveChanged();
    void autoReadMessagesChanged();
    void voiceVolumeChanged();
    void speechRateChanged();
    void activeAssistantChanged();
    void quickRepliesChanged();
    void statusMessageChanged();

    /**
     * Emitted when voice command is recognized
     * @param command: Recognized command text
     */
    void commandRecognized(const QString &command);

    /**
     * Emitted when message is read aloud
     * @param sender: Sender name
     * @param message: Message text
     */
    void messageRead(const QString &sender, const QString &message);

    /**
     * Emitted when quick reply is sent
     * @param text: Reply text
     */
    void replySent(const QString &text);

    /**
     * Emitted on errors
     * @param message: Error description
     */
    void error(const QString &message);

private slots:
#ifndef Q_OS_WIN
    /**
     * Handle Bluetooth socket events
     */
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();
    void onSocketReadyRead();
#endif

private:
    // ========== HELPER METHODS ==========

    /**
     * Set status message and emit signal
     */
    void setStatusMessage(const QString &msg);

    /**
     * Send HFP command to phone
     * @param command: AT command string
     */
    void sendHFPCommand(const QString &command);

    /**
     * Parse HFP response from phone
     * @param response: Response data
     */
    void parseHFPResponse(const QString &response);

    // ========== MEMBER VARIABLES ==========

    // Connection
    bool m_isConnected;
    QString m_deviceAddress;

    // Voice state
    bool m_isListening;
    bool m_isVoiceActive;

    // Settings
    bool m_autoReadMessages;
    int m_voiceVolume;
    double m_speechRate;

    // Assistant info
    QString m_activeAssistant;  // "Siri", "Google Assistant", etc.

    // Quick replies
    QStringList m_quickReplies;

    // Status
    QString m_statusMessage;

    // Text-to-Speech (optional)
#if HAS_TEXT_TO_SPEECH
    QTextToSpeech *m_tts;
#else
    void *m_tts;  // Placeholder
#endif

#ifndef Q_OS_WIN
    // Bluetooth (real mode)
    QBluetoothAddress m_bluetoothAddress;
    QBluetoothSocket *m_socket;
#endif
};

#endif // VOICEASSISTANT_H
