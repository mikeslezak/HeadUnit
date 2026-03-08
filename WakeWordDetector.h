#ifndef WAKEWORDDETECTOR_H
#define WAKEWORDDETECTOR_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QAudioSource>
#include <QIODevice>

// Forward declare Porcupine types
typedef struct pv_porcupine pv_porcupine_t;

/**
 * WakeWordDetector - Offline Wake Word Detection using Porcupine
 *
 * This class provides continuous wake word detection for "Hey Sammy" (initially using
 * a built-in wake word until custom training is complete).
 *
 * Features:
 * - Runs completely offline (no internet required)
 * - Low CPU usage, optimized for embedded devices
 * - Configurable sensitivity
 * - Continuous audio monitoring via microphone
 * - Qt signal-based integration with VoiceAssistant
 *
 * Usage:
 *   WakeWordDetector *detector = new WakeWordDetector(this);
 *   connect(detector, &WakeWordDetector::wakeWordDetected, ...);
 *   detector->start();
 */
class WakeWordDetector : public QObject
{
    Q_OBJECT

    // ========== PROPERTIES ==========

    /**
     * Running State
     * True when actively listening for wake word
     */
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)

    /**
     * Sensitivity
     * Range: 0.0 to 1.0 (higher = more sensitive, more false positives)
     */
    Q_PROPERTY(float sensitivity READ sensitivity WRITE setSensitivity NOTIFY sensitivityChanged)

    /**
     * Current Wake Word
     * Name of the active wake word (e.g., "jarvis", "computer")
     */
    Q_PROPERTY(QString wakeWord READ wakeWord WRITE setWakeWord NOTIFY wakeWordChanged)

    /**
     * Status Message
     * Current status for debugging/display
     */
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    /**
     * Initialized
     * True if Porcupine library initialized successfully
     */
    Q_PROPERTY(bool isInitialized READ isInitialized NOTIFY initializedChanged)

public:
    explicit WakeWordDetector(QObject *parent = nullptr);
    ~WakeWordDetector();

    // ========== PROPERTY GETTERS ==========

    bool isRunning() const { return m_isRunning; }
    float sensitivity() const { return m_sensitivity; }
    QString wakeWord() const { return m_wakeWord; }
    QString statusMessage() const { return m_statusMessage; }
    bool isInitialized() const { return m_isInitialized; }

public slots:
    // ========== CONTROL ==========

    /**
     * Start wake word detection
     * Begins continuous microphone monitoring
     */
    void start();

    /**
     * Stop wake word detection
     * Releases microphone and pauses monitoring
     */
    void stop();

    /**
     * Pause detection temporarily
     * Keeps Porcupine loaded but stops audio processing
     */
    void pause();

    /**
     * Resume detection after pause
     */
    void resume();

    // ========== SETTINGS ==========

    /**
     * Set wake word sensitivity
     * @param sensitivity: 0.0 (less sensitive) to 1.0 (more sensitive)
     */
    void setSensitivity(float sensitivity);

    /**
     * Set wake word to detect
     * @param keyword: Wake word name (e.g., "jarvis", "computer", "hey_google")
     *
     * Note: Currently limited to built-in Porcupine keywords.
     * Custom "Hey Sammy" will be added via Picovoice Console training.
     */
    void setWakeWord(const QString &keyword);

    /**
     * Set Picovoice access key
     * Required for Porcupine initialization
     * Get from: https://console.picovoice.ai/
     */
    void setAccessKey(const QString &accessKey);

signals:
    // ========== SIGNALS ==========

    void runningChanged();
    void sensitivityChanged();
    void wakeWordChanged();
    void statusMessageChanged();
    void initializedChanged();

    /**
     * Emitted when wake word is detected
     * @param keyword: Name of detected wake word
     * @param confidence: Detection confidence (currently always returns index)
     */
    void wakeWordDetected(const QString &keyword, int confidence);

    /**
     * Emitted on errors
     * @param message: Error description
     */
    void error(const QString &message);

private slots:
    /**
     * Handle incoming audio data from microphone
     */
    void onAudioReady();

private:
    // ========== HELPER METHODS ==========

    /**
     * Initialize Porcupine library
     * @return true on success
     */
    bool initializePorcupine();

    /**
     * Release Porcupine resources
     */
    void cleanupPorcupine();

    /**
     * Set status message and emit signal
     */
    void setStatusMessage(const QString &msg);

    /**
     * Get path to Porcupine model file
     */
    QString getModelPath() const;

    /**
     * Get path to keyword file for current wake word
     */
    QString getKeywordPath() const;

    /**
     * Process audio frame through Porcupine
     * @param audioFrame: PCM audio data (int16_t samples)
     * @return keyword index if detected, -1 otherwise
     */
    int32_t processAudioFrame(const int16_t *audioFrame);

    // ========== MEMBER VARIABLES ==========

    // Porcupine
    pv_porcupine_t *m_porcupine;
    QString m_accessKey;
    bool m_isInitialized;

    // State
    bool m_isRunning;
    bool m_isPaused;

    // Settings
    float m_sensitivity;
    QString m_wakeWord;

    // Status
    QString m_statusMessage;

    // Audio Input
    QAudioSource *m_audioSource;
    QIODevice *m_audioDevice;

    // Audio Processing
    QByteArray m_audioBuffer;
    int32_t m_frameLength;      // Samples per frame (from Porcupine)
    int32_t m_sampleRate;       // Sample rate (from Porcupine)

    // Debouncing
    qint64 m_lastDetectionTime;  // Timestamp of last wake word detection (msec since epoch)

    // Thread Safety
    QMutex m_mutex;
};

#endif // WAKEWORDDETECTOR_H
