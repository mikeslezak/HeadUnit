#ifndef GOOGLESPEECHRECOGNIZER_H
#define GOOGLESPEECHRECOGNIZER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QAudioSource>
#include <QAudioFormat>
#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMediaDevices>

/**
 * GoogleSpeechRecognizer - Google Cloud Speech-to-Text Integration
 *
 * This class handles real-time speech recognition using Google Cloud Speech-to-Text API.
 * It captures audio from the microphone and sends it to Google for transcription.
 *
 * Features:
 * - Real-time streaming speech recognition
 * - Automatic silence detection and stop
 * - Voice activity detection
 * - High-quality recognition using Google's neural networks
 * - Simple REST API integration
 *
 * Usage:
 *   GoogleSpeechRecognizer *recognizer = new GoogleSpeechRecognizer(this);
 *   recognizer->setApiKey("AIza...");
 *   connect(recognizer, &GoogleSpeechRecognizer::transcriptionReceived, ...);
 *   recognizer->startListening();
 */
class GoogleSpeechRecognizer : public QObject
{
    Q_OBJECT

    // ========== PROPERTIES ==========

    /**
     * Listening State
     * True when actively recording audio
     */
    Q_PROPERTY(bool isListening READ isListening NOTIFY listeningChanged)

    /**
     * Processing State
     * True when sending audio to Google for recognition
     */
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)

    /**
     * Status Message
     * Current status for debugging/display
     */
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit GoogleSpeechRecognizer(QObject *parent = nullptr);
    ~GoogleSpeechRecognizer();

    // ========== PROPERTY GETTERS ==========

    bool isListening() const { return m_isListening; }
    bool isProcessing() const { return m_isProcessing; }
    QString statusMessage() const { return m_statusMessage; }

public slots:
    // ========== CONFIGURATION ==========

    /**
     * Set Google Cloud API key
     * @param apiKey: Your Google Cloud API key
     */
    void setApiKey(const QString &apiKey);

    /**
     * Set language code for recognition
     * @param languageCode: BCP-47 language code (e.g., "en-US", "es-ES")
     */
    void setLanguageCode(const QString &languageCode);

    /**
     * Set maximum listening duration in milliseconds
     * @param maxDuration: Maximum recording time (default: 30000ms = 30 seconds)
     */
    void setMaxListeningDuration(int maxDuration);

    // ========== RECORDING CONTROL ==========

    /**
     * Start listening and recording audio
     * Records until stopListening() is called or silence is detected
     */
    void startListening();

    /**
     * Stop listening and process recorded audio
     * Sends audio to Google for transcription
     */
    void stopListening();

    /**
     * Cancel current recording
     * Discards recorded audio without processing
     */
    void cancel();

signals:
    // ========== SIGNALS ==========

    void listeningChanged();
    void processingChanged();
    void statusMessageChanged();

    /**
     * Emitted when transcription is received from Google
     * @param transcript: The recognized text
     * @param isFinal: True if this is the final result
     */
    void transcriptionReceived(const QString &transcript, bool isFinal);

    /**
     * Emitted on errors
     * @param message: Error description
     */
    void error(const QString &message);

    /**
     * Emitted when recording starts
     */
    void recordingStarted();

    /**
     * Emitted when recording stops
     */
    void recordingStopped();

private slots:
    /**
     * Handle audio data ready
     */
    void onAudioDataReady();

    /**
     * Handle network response
     */
    void onNetworkReply(QNetworkReply *reply);

    /**
     * Handle network errors
     */
    void onNetworkError(QNetworkReply::NetworkError error);

    /**
     * Check for silence timeout
     */
    void onSilenceTimeout();

    /**
     * Check for max duration timeout
     */
    void onMaxDurationTimeout();

private:
    // ========== HELPER METHODS ==========

    /**
     * Initialize audio input
     */
    bool initializeAudio();

    /**
     * Send audio data to Google Speech API
     */
    void sendToGoogle();

    /**
     * Convert audio to base64 for API
     */
    QString audioToBase64() const;

    /**
     * Set status message and emit signal
     */
    void setStatusMessage(const QString &msg);

    /**
     * Reset internal state
     */
    void reset();

    // ========== MEMBER VARIABLES ==========

    // Network
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;

    // Configuration
    QString m_apiKey;
    QString m_languageCode;
    int m_maxListeningDuration;

    // Audio
    QAudioSource *m_audioSource;
    QIODevice *m_audioDevice;
    QByteArray m_audioBuffer;
    QAudioFormat m_audioFormat;

    // State
    bool m_isListening;
    bool m_isProcessing;
    QString m_statusMessage;

    // Timers
    QTimer *m_silenceTimer;
    QTimer *m_maxDurationTimer;

    // Silence detection
    qint64 m_lastAudioTime;
    static constexpr int SILENCE_TIMEOUT_MS = 2000;  // 2 seconds of silence

    // Constants
    static constexpr const char* API_ENDPOINT = "https://speech.googleapis.com/v1/speech:recognize";
    static constexpr int DEFAULT_MAX_DURATION = 30000;  // 30 seconds
    static constexpr int SAMPLE_RATE = 16000;           // 16kHz (required by Google)
    static constexpr int CHANNEL_COUNT = 1;             // Mono
    static constexpr int SAMPLE_SIZE = 16;              // 16-bit
};

#endif // GOOGLESPEECHRECOGNIZER_H
