#ifndef GOOGLESTT_H
#define GOOGLESTT_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVector>

/**
 * GoogleSTT - Google Cloud Speech-to-Text Integration
 *
 * This class converts audio to text using Google Cloud Speech-to-Text API.
 * It provides much better accuracy for names and proper nouns compared to
 * offline STT engines like Picovoice Leopard.
 *
 * Features:
 * - High accuracy speech recognition
 * - Support for speech context hints (contact names)
 * - Multiple language support
 * - Automatic punctuation
 *
 * Usage:
 *   GoogleSTT *stt = new GoogleSTT(this);
 *   stt->setApiKey("AIza...");
 *   connect(stt, &GoogleSTT::transcriptionReady, ...);
 *   stt->transcribe(audioSamples);
 */
class GoogleSTT : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit GoogleSTT(QObject *parent = nullptr);
    ~GoogleSTT();

    // Property getters
    bool isProcessing() const { return m_isProcessing; }
    QString statusMessage() const { return m_statusMessage; }

public slots:
    /**
     * Set Google Cloud API key
     * @param apiKey: Your Google Cloud API key (same as TTS)
     */
    void setApiKey(const QString &apiKey);

    /**
     * Set language code for recognition
     * @param languageCode: BCP-47 language code (default: "en-US")
     */
    void setLanguageCode(const QString &languageCode);

    /**
     * Set speech context hints (e.g., contact names) to improve accuracy
     * @param phrases: List of words/phrases likely to be spoken
     */
    void setSpeechContextHints(const QStringList &phrases);

    /**
     * Transcribe audio samples
     * @param samples: 16-bit PCM audio samples at 16kHz mono
     */
    void transcribe(const QVector<int16_t> &samples);

    /**
     * Transcribe raw audio data
     * @param audioData: Raw 16-bit PCM audio data at 16kHz mono
     */
    void transcribeRaw(const QByteArray &audioData);

    /**
     * Cancel current transcription request
     */
    void cancel();

signals:
    void processingChanged();
    void statusMessageChanged();

    /**
     * Emitted when transcription completes
     * @param text: The transcribed text
     * @param confidence: Recognition confidence (0.0 to 1.0)
     */
    void transcriptionReady(const QString &text, float confidence);

    /**
     * Emitted on errors
     * @param message: Error description
     */
    void error(const QString &message);

private slots:
    void onNetworkReply(QNetworkReply *reply);
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    void sendToGoogle(const QByteArray &audioData);
    void setStatusMessage(const QString &msg);
    QString encodeAudioToBase64(const QByteArray &audioData);

    // Network
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;

    // Configuration
    QString m_apiKey;
    QString m_languageCode;
    QStringList m_speechContextHints;

    // State
    bool m_isProcessing;
    QString m_statusMessage;

    // Constants
    static constexpr const char* API_ENDPOINT = "https://speech.googleapis.com/v1/speech:recognize";
    static constexpr int SAMPLE_RATE = 16000;
};

#endif // GOOGLESTT_H
