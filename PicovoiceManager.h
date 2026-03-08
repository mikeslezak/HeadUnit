#ifndef PICOVOICEMANAGER_H
#define PICOVOICEMANAGER_H

#include <QObject>
#include <QAudioSource>
#include <QIODevice>
#include <QByteArray>
#include <QVector>
#include <QVariantMap>
#include <QMutex>
#include <QString>

// Forward declaration for Google STT
class GoogleSTT;

// Forward declarations for Picovoice C structures
typedef struct pv_porcupine pv_porcupine_t;
typedef struct pv_rhino pv_rhino_t;
typedef struct pv_leopard pv_leopard_t;
typedef struct pv_koala pv_koala_t;

class PicovoiceManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool isInitialized READ isInitialized NOTIFY initializedChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(float sensitivity READ sensitivity WRITE setSensitivity NOTIFY sensitivityChanged)
    Q_PROPERTY(QString wakeWord READ wakeWord WRITE setWakeWord NOTIFY wakeWordChanged)

public:
    explicit PicovoiceManager(QObject *parent = nullptr);
    ~PicovoiceManager();

    // Control methods
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void onReadyPromptFinished();  // Call after TTS prompt completes

    // Configuration methods
    void setAccessKey(const QString &key);
    void setSensitivity(float sensitivity);
    void setWakeWord(const QString &keyword);
    void setRhinoContextPath(const QString &path);
    void setGoogleApiKey(const QString &key);
    void setSpeechContextHints(const QStringList &hints);

    // Getters
    bool isRunning() const { return m_isRunning; }
    bool isInitialized() const { return m_isInitialized; }
    QString statusMessage() const { return m_statusMessage; }
    float sensitivity() const { return m_sensitivity; }
    QString wakeWord() const { return m_wakeWord; }

signals:
    void wakeWordDetected(const QString &keyword);
    void intentDetected(const QString &intent, const QVariantMap &slots);
    void transcriptionReady(const QString &text);
    void error(const QString &message);
    void statusMessageChanged();
    void runningChanged();
    void initializedChanged();
    void sensitivityChanged();
    void wakeWordChanged();

private slots:
    void onAudioReady();
    void onGoogleTranscriptionReady(const QString &text, float confidence);
    void onGoogleError(const QString &message);

private:
    // State machine
    enum State {
        Listening,            // Listening for wake word
        WaitingForReadyPrompt,// Wake word detected, waiting for TTS ready prompt to finish
        WaitingForCommand,    // Ready prompt finished, processing with Rhino
        ProcessingSpeech      // Rhino didn't understand, accumulating for Leopard
    };
    State m_state;

    // Picovoice components
    pv_porcupine_t *m_porcupine;
    pv_rhino_t *m_rhino;
    pv_leopard_t *m_leopard;
    pv_koala_t *m_koala;

    // Google Cloud STT (primary STT engine)
    GoogleSTT *m_googleSTT;
    QString m_googleApiKey;
    QStringList m_speechContextHints;

    // Configuration
    QString m_accessKey;
    QString m_wakeWord;
    QString m_rhinoContextPath;
    float m_sensitivity;

    // Audio pipeline
    QAudioSource *m_audioSource;
    QIODevice *m_audioDevice;
    QByteArray m_audioBuffer;

    // Audio parameters
    int32_t m_frameLength;          // Frame length for Porcupine/Rhino/Koala
    int32_t m_sampleRate;           // Sample rate (16kHz)
    int32_t m_koalaFrameLength;     // Koala frame length
    int32_t m_koalaDelaySamples;    // Koala processing delay

    // State tracking
    bool m_isRunning;
    bool m_isPaused;
    bool m_isInitialized;
    QString m_statusMessage;

    // Speech buffer for Leopard (when Rhino doesn't understand)
    QVector<int16_t> m_speechBuffer;
    qint64 m_speechStartTime;
    static const int MAX_SPEECH_DURATION_MS = 10000;  // 10 seconds max

    // Voice Activity Detection (VAD) for faster response
    qint64 m_lastVoiceActivityTime;
    static const int SILENCE_THRESHOLD_MS = 1500;     // 1.5 seconds of silence triggers finalization
    static const int16_t SILENCE_ENERGY_THRESHOLD = 500;  // RMS energy below this = silence
    static const int MIN_SPEECH_DURATION_MS = 500;    // Minimum speech before allowing silence detection

    // Debouncing
    qint64 m_lastDetectionTime;
    static const int DETECTION_DEBOUNCE_MS = 1000;

    // Thread safety
    QMutex m_mutex;

    // Initialization methods
    bool initializePorcupine();
    bool initializeRhino();
    bool initializeLeopard();
    bool initializeKoala();
    void cleanup();
    void cleanupPorcupine();
    void cleanupRhino();
    void cleanupLeopard();
    void cleanupKoala();

    // Audio processing methods
    void processAudioFrame(const int16_t *frame, int32_t length);
    void processWakeWord(const int16_t *frame);
    void processRhinoIntent(const int16_t *frame);
    void finalizeLeopardTranscription();

    // Helper methods
    QString getModelPath(const QString &component) const;
    QString getKeywordPath() const;
    void setStatusMessage(const QString &msg);
    void resetToListening();
    int16_t calculateFrameEnergy(const int16_t *frame, int32_t length) const;
};

#endif // PICOVOICEMANAGER_H
