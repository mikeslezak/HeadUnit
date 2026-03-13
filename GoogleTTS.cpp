#include "GoogleTTS.h"
#include <QDebug>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QBuffer>

GoogleTTS::GoogleTTS(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_voiceName("en-US-Studio-O")  // Studio: highest quality female US English voice
    , m_languageCode("en-US")
    , m_speakingRate(1.0)
    , m_pitch(0.0)
    , m_audioSink(nullptr)
    , m_audioBuffer(nullptr)
    , m_isSpeaking(false)
    , m_isProcessing(false)
    , m_statusMessage("Not configured")
{
    qDebug() << "GoogleTTS: Initializing...";

    // Load API key from environment first, fall back to QSettings
    m_apiKey = qEnvironmentVariable("GOOGLE_API_KEY");
    if (m_apiKey.isEmpty()) {
        QSettings settings;
        m_apiKey = settings.value("google/apiKey", "").toString();
    }

    if (!m_apiKey.isEmpty()) {
        setStatusMessage("Ready - API key configured");
        qDebug() << "GoogleTTS: API key loaded";
    } else {
        setStatusMessage("API key required");
        qDebug() << "GoogleTTS: No API key configured";
    }

    // Connect network manager signals
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &GoogleTTS::onNetworkReply);

    qDebug() << "GoogleTTS: Initialization complete";
}

GoogleTTS::~GoogleTTS()
{
    if (m_isSpeaking) {
        stop();
    }

    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }

    if (m_audioSink) {
        delete m_audioSink;
    }

    if (m_audioBuffer) {
        delete m_audioBuffer;
    }
}

// ========================================================================
// CONFIGURATION
// ========================================================================

void GoogleTTS::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();

    if (!m_apiKey.isEmpty()) {
        setStatusMessage("API key configured");
        qDebug() << "GoogleTTS: API key set";
    } else {
        setStatusMessage("API key cleared");
        qDebug() << "GoogleTTS: API key cleared";
    }
}

void GoogleTTS::setVoiceName(const QString &voiceName)
{
    m_voiceName = voiceName;
    qDebug() << "GoogleTTS: Voice name set to" << m_voiceName;
}

void GoogleTTS::setLanguageCode(const QString &languageCode)
{
    m_languageCode = languageCode;
    qDebug() << "GoogleTTS: Language code set to" << m_languageCode;
}

void GoogleTTS::setSpeakingRate(double rate)
{
    m_speakingRate = qBound(0.25, rate, 4.0);
    qDebug() << "GoogleTTS: Speaking rate set to" << m_speakingRate;
}

void GoogleTTS::setPitch(double pitch)
{
    m_pitch = qBound(-20.0, pitch, 20.0);
    qDebug() << "GoogleTTS: Pitch set to" << m_pitch;
}

// ========================================================================
// SPEECH CONTROL
// ========================================================================

void GoogleTTS::speak(const QString &text)
{
    if (m_apiKey.isEmpty()) {
        emit error("API key not configured");
        setStatusMessage("Error: No API key");
        return;
    }

    if (text.isEmpty()) {
        qWarning() << "GoogleTTS: Empty text provided";
        return;
    }

    // Stop any current audio playback safely
    if (m_isSpeaking || m_audioSink || m_audioBuffer || m_currentReply) {
        qDebug() << "GoogleTTS: Stopping current speech before starting new one";
        stop();
    }

    qDebug() << "GoogleTTS: Speaking:" << text;

    // Send to Google for synthesis
    sendToGoogle(text);
}

void GoogleTTS::stop()
{
    qDebug() << "GoogleTTS: Stopping speech...";

    // Abort any pending network request first
    if (m_currentReply) {
        disconnect(m_currentReply, &QNetworkReply::errorOccurred,
                   this, &GoogleTTS::onNetworkError);
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    // Stop audio sink first (disconnecting signal prevents re-entry)
    if (m_audioSink) {
        disconnect(m_audioSink, &QAudioSink::stateChanged,
                   this, &GoogleTTS::onAudioStateChanged);
        m_audioSink->stop();
        // Synchronous delete since we've stopped and disconnected
        delete m_audioSink;
        m_audioSink = nullptr;
    }

    // Now safe to delete buffer since audio sink is gone
    if (m_audioBuffer) {
        m_audioBuffer->close();
        delete m_audioBuffer;
        m_audioBuffer = nullptr;
    }

    // Now safe to clear audio data since buffer is gone
    m_audioData.clear();

    // Update state
    m_isSpeaking = false;
    m_isProcessing = false;
    emit speakingChanged();
    emit processingChanged();
    setStatusMessage("Stopped");
}

// ========================================================================
// GOOGLE TTS API
// ========================================================================

void GoogleTTS::sendToGoogle(const QString &text)
{
    m_isProcessing = true;
    emit processingChanged();
    setStatusMessage("Synthesizing speech...");

    qDebug() << "GoogleTTS: Sending text to Google TTS API";

    // Build JSON request
    QJsonObject input;
    input["text"] = text;

    QJsonObject voice;
    voice["languageCode"] = m_languageCode;
    voice["name"] = m_voiceName;

    QJsonObject audioConfig;
    audioConfig["audioEncoding"] = "LINEAR16";
    audioConfig["sampleRateHertz"] = SAMPLE_RATE;
    audioConfig["speakingRate"] = m_speakingRate;
    audioConfig["pitch"] = m_pitch;

    QJsonObject request;
    request["input"] = input;
    request["voice"] = voice;
    request["audioConfig"] = audioConfig;

    // Create network request
    QString urlWithKey = QString("%1?key=%2").arg(API_ENDPOINT, m_apiKey);
    QUrl url(urlWithKey);
    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setTransferTimeout(15000);

    // Send request
    QByteArray requestData = QJsonDocument(request).toJson(QJsonDocument::Compact);
    qDebug() << "GoogleTTS: Request size:" << requestData.size() << "bytes";

    m_currentReply = m_networkManager->post(networkRequest, requestData);

    // Connect reply signals
    connect(m_currentReply, &QNetworkReply::errorOccurred,
            this, &GoogleTTS::onNetworkError);
}

void GoogleTTS::onNetworkReply(QNetworkReply *reply)
{
    // Safety check - if this reply was already handled or aborted, skip it
    if (reply != m_currentReply) {
        reply->deleteLater();
        return;
    }

    m_isProcessing = false;
    emit processingChanged();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();

    qDebug() << "GoogleTTS: HTTP Status Code:" << statusCode;
    qDebug() << "GoogleTTS: Response size:" << responseData.size() << "bytes";

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        qWarning() << "GoogleTTS: Network error:" << errorMsg;
        qWarning() << "GoogleTTS: Response body:" << responseData;
        emit error("TTS error: " + errorMsg);
        setStatusMessage("Error: " + errorMsg);
        reply->deleteLater();
        m_currentReply = nullptr;
        reset();
        emit speechFinished();
        return;
    }

    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "GoogleTTS: Invalid JSON response";
        emit error("Invalid response from Google");
        setStatusMessage("Error: Invalid response");
        reply->deleteLater();
        m_currentReply = nullptr;
        reset();
        return;
    }

    QJsonObject response = doc.object();

    // Check for API errors
    if (response.contains("error")) {
        QJsonObject errorObj = response["error"].toObject();
        QString errorMessage = errorObj["message"].toString();
        qWarning() << "GoogleTTS: API error:" << errorMessage;
        emit error("Google TTS error: " + errorMessage);
        setStatusMessage("Error: " + errorMessage);
        reply->deleteLater();
        m_currentReply = nullptr;
        reset();
        return;
    }

    // Extract audio content
    if (response.contains("audioContent")) {
        QString audioContentBase64 = response["audioContent"].toString();
        QByteArray audioData = QByteArray::fromBase64(audioContentBase64.toLatin1());

        qDebug() << "GoogleTTS: Received audio data:" << audioData.size() << "bytes";

        // Play the audio
        playAudio(audioData);
    } else {
        qWarning() << "GoogleTTS: No audioContent in response";
        emit error("No audio data received");
        setStatusMessage("Error: No audio data");
        emit speechFinished();
    }

    reply->deleteLater();
    m_currentReply = nullptr;
}

void GoogleTTS::onNetworkError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);

    if (!m_currentReply) {
        return;
    }

    QString errorMsg = m_currentReply->errorString();
    qWarning() << "GoogleTTS: Network error:" << errorMsg;

    // Null out so the finished handler (onNetworkReply) skips re-processing
    m_currentReply = nullptr;

    m_isProcessing = false;
    emit processingChanged();
    emit this->error("Connection error: " + errorMsg);
    setStatusMessage("Error: " + errorMsg);

    reset();
}

// ========================================================================
// AUDIO PLAYBACK
// ========================================================================

bool GoogleTTS::initializeAudio()
{
    // Setup audio format (LINEAR16, 24kHz mono - from Google TTS)
    m_audioFormat.setSampleRate(SAMPLE_RATE);
    m_audioFormat.setChannelCount(CHANNEL_COUNT);
    m_audioFormat.setSampleFormat(QAudioFormat::Int16);

    // Get default audio output device
    QAudioDevice audioDevice = QMediaDevices::defaultAudioOutput();
    if (audioDevice.isNull()) {
        qWarning() << "GoogleTTS: No audio output device found";
        return false;
    }

    // Check if format is supported
    if (!audioDevice.isFormatSupported(m_audioFormat)) {
        qWarning() << "GoogleTTS: Audio format not supported";
        qWarning() << "Requested format:" << m_audioFormat;
        return false;
    }

    // Create audio sink
    m_audioSink = new QAudioSink(audioDevice, m_audioFormat, this);

    // Connect state changed signal
    connect(m_audioSink, &QAudioSink::stateChanged,
            this, &GoogleTTS::onAudioStateChanged);

    qDebug() << "GoogleTTS: Audio initialized successfully";
    qDebug() << "  Sample rate:" << m_audioFormat.sampleRate();
    qDebug() << "  Channels:" << m_audioFormat.channelCount();
    qDebug() << "  Sample format:" << m_audioFormat.sampleFormat();

    return true;
}

void GoogleTTS::playAudio(const QByteArray &audioData)
{
    qDebug() << "GoogleTTS: Playing audio...";

    // Clean up any lingering audio objects first (synchronously)
    if (m_audioSink) {
        disconnect(m_audioSink, &QAudioSink::stateChanged,
                   this, &GoogleTTS::onAudioStateChanged);
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
    }

    if (m_audioBuffer) {
        m_audioBuffer->close();
        delete m_audioBuffer;
        m_audioBuffer = nullptr;
    }

    // Initialize audio if needed
    if (!initializeAudio()) {
        emit error("Failed to initialize audio output");
        setStatusMessage("Error: Audio initialization failed");
        emit speechFinished();
        return;
    }

    // Store audio data in buffer - make a copy to ensure ownership
    m_audioData = audioData;
    m_audioBuffer = new QBuffer(&m_audioData, this);
    m_audioBuffer->open(QIODevice::ReadOnly);

    m_isSpeaking = true;
    emit speakingChanged();
    emit speechStarted();
    setStatusMessage("Speaking...");

    // Start playback
    m_audioSink->start(m_audioBuffer);

    qDebug() << "GoogleTTS: Audio playback started";
}

void GoogleTTS::onAudioStateChanged(QAudio::State state)
{
    qDebug() << "GoogleTTS: Audio state changed:" << state;

    switch (state) {
    case QAudio::IdleState:
        // Playback finished
        if (m_isSpeaking) {
            qDebug() << "GoogleTTS: Playback finished";
            emit speechFinished();
            setStatusMessage("Ready");
            reset();
        }
        break;

    case QAudio::StoppedState:
        // Playback stopped (either finished or error)
        if (m_isSpeaking) {
            if (m_audioSink && m_audioSink->error() != QAudio::NoError) {
                qWarning() << "GoogleTTS: Audio error:" << m_audioSink->error();
                emit error("Audio playback error");
                setStatusMessage("Error: Playback failed");
            }
            reset();
        }
        break;

    case QAudio::ActiveState:
        qDebug() << "GoogleTTS: Playback active";
        break;

    case QAudio::SuspendedState:
        qDebug() << "GoogleTTS: Playback suspended";
        break;
    }
}

// ========================================================================
// HELPER METHODS
// ========================================================================

void GoogleTTS::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "GoogleTTS:" << msg;
    }
}

void GoogleTTS::reset()
{
    // Schedule cleanup to happen after we exit the current callback
    // This is important when reset() is called from onAudioStateChanged
    QMetaObject::invokeMethod(this, [this]() {
        // Disconnect first to prevent any further callbacks
        if (m_audioSink) {
            disconnect(m_audioSink, &QAudioSink::stateChanged,
                       this, &GoogleTTS::onAudioStateChanged);
            delete m_audioSink;
            m_audioSink = nullptr;
        }

        if (m_audioBuffer) {
            m_audioBuffer->close();
            delete m_audioBuffer;
            m_audioBuffer = nullptr;
        }

        m_audioData.clear();
    }, Qt::QueuedConnection);

    // Update state immediately
    m_isSpeaking = false;
    m_isProcessing = false;
    emit speakingChanged();
    emit processingChanged();
}
