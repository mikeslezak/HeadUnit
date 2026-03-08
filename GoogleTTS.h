#ifndef GOOGLETTS_H
#define GOOGLETTS_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAudioSink>
#include <QAudioFormat>
#include <QBuffer>
#include <QMediaDevices>

/**
 * GoogleTTS - Google Cloud Text-to-Speech Integration
 *
 * This class converts text to natural-sounding speech using Google Cloud TTS API.
 * It downloads synthesized audio and plays it through the default audio output.
 *
 * Features:
 * - High-quality neural voices (WaveNet)
 * - Multiple languages and voice options
 * - Adjustable speaking rate and pitch
 * - Simple REST API integration
 * - Automatic audio playback
 *
 * Usage:
 *   GoogleTTS *tts = new GoogleTTS(this);
 *   tts->setApiKey("AIza...");
 *   connect(tts, &GoogleTTS::speechFinished, ...);
 *   tts->speak("Hello, world!");
 */
class GoogleTTS : public QObject
{
    Q_OBJECT

    // ========== PROPERTIES ==========

    /**
     * Speaking State
     * True when currently synthesizing or playing audio
     */
    Q_PROPERTY(bool isSpeaking READ isSpeaking NOTIFY speakingChanged)

    /**
     * Processing State
     * True when sending request to Google TTS API
     */
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)

    /**
     * Status Message
     * Current status for debugging/display
     */
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit GoogleTTS(QObject *parent = nullptr);
    ~GoogleTTS();

    // ========== PROPERTY GETTERS ==========

    bool isSpeaking() const { return m_isSpeaking; }
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
     * Set voice name
     * @param voiceName: Voice identifier (e.g., "en-US-Neural2-F" for female, "en-US-Neural2-J" for male)
     * Full list: https://cloud.google.com/text-to-speech/docs/voices
     */
    void setVoiceName(const QString &voiceName);

    /**
     * Set language code
     * @param languageCode: BCP-47 language code (e.g., "en-US", "es-ES")
     */
    void setLanguageCode(const QString &languageCode);

    /**
     * Set speaking rate
     * @param rate: Speed multiplier (0.25 to 4.0, default: 1.0)
     */
    void setSpeakingRate(double rate);

    /**
     * Set voice pitch
     * @param pitch: Pitch adjustment in semitones (-20.0 to 20.0, default: 0.0)
     */
    void setPitch(double pitch);

    // ========== SPEECH CONTROL ==========

    /**
     * Convert text to speech and play it
     * @param text: The text to speak
     */
    void speak(const QString &text);

    /**
     * Stop current speech playback
     */
    void stop();

signals:
    // ========== SIGNALS ==========

    void speakingChanged();
    void processingChanged();
    void statusMessageChanged();

    /**
     * Emitted when speech synthesis completes
     */
    void speechFinished();

    /**
     * Emitted when speech playback starts
     */
    void speechStarted();

    /**
     * Emitted on errors
     * @param message: Error description
     */
    void error(const QString &message);

private slots:
    /**
     * Handle network response from Google TTS API
     */
    void onNetworkReply(QNetworkReply *reply);

    /**
     * Handle network errors
     */
    void onNetworkError(QNetworkReply::NetworkError error);

    /**
     * Handle audio sink state changes
     */
    void onAudioStateChanged(QAudio::State state);

private:
    // ========== HELPER METHODS ==========

    /**
     * Send text to Google TTS API
     */
    void sendToGoogle(const QString &text);

    /**
     * Initialize audio output
     */
    bool initializeAudio();

    /**
     * Play audio data
     */
    void playAudio(const QByteArray &audioData);

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
    QString m_voiceName;
    QString m_languageCode;
    double m_speakingRate;
    double m_pitch;

    // Audio
    QAudioSink *m_audioSink;
    QBuffer *m_audioBuffer;
    QAudioFormat m_audioFormat;
    QByteArray m_audioData;

    // State
    bool m_isSpeaking;
    bool m_isProcessing;
    QString m_statusMessage;

    // Constants
    static constexpr const char* API_ENDPOINT = "https://texttospeech.googleapis.com/v1/text:synthesize";
    static constexpr int SAMPLE_RATE = 24000;       // 24kHz (default for LINEAR16)
    static constexpr int CHANNEL_COUNT = 1;         // Mono
};

#endif // GOOGLETTS_H
