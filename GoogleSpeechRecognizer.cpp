#include "GoogleSpeechRecognizer.h"
#include <QDebug>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QAudioSource>
#include <QBuffer>

GoogleSpeechRecognizer::GoogleSpeechRecognizer(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_languageCode("en-US")
    , m_maxListeningDuration(DEFAULT_MAX_DURATION)
    , m_audioSource(nullptr)
    , m_audioDevice(nullptr)
    , m_isListening(false)
    , m_isProcessing(false)
    , m_statusMessage("Not configured")
    , m_silenceTimer(new QTimer(this))
    , m_maxDurationTimer(new QTimer(this))
    , m_lastAudioTime(0)
{
    qDebug() << "GoogleSpeechRecognizer: Initializing...";

    // Load saved API key (if any)
    QSettings settings;
    m_apiKey = settings.value("google/apiKey", "").toString();

    if (!m_apiKey.isEmpty()) {
        setStatusMessage("Ready - API key configured");
        qDebug() << "GoogleSpeechRecognizer: API key loaded from settings";
    } else {
        setStatusMessage("API key required");
        qDebug() << "GoogleSpeechRecognizer: No API key configured";
    }

    // Connect network manager signals
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &GoogleSpeechRecognizer::onNetworkReply);

    // Setup timers
    m_silenceTimer->setSingleShot(true);
    m_silenceTimer->setInterval(SILENCE_TIMEOUT_MS);
    connect(m_silenceTimer, &QTimer::timeout,
            this, &GoogleSpeechRecognizer::onSilenceTimeout);

    m_maxDurationTimer->setSingleShot(true);
    connect(m_maxDurationTimer, &QTimer::timeout,
            this, &GoogleSpeechRecognizer::onMaxDurationTimeout);

    qDebug() << "GoogleSpeechRecognizer: Initialization complete";
}

GoogleSpeechRecognizer::~GoogleSpeechRecognizer()
{
    if (m_isListening) {
        cancel();
    }

    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

// ========================================================================
// CONFIGURATION
// ========================================================================

void GoogleSpeechRecognizer::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();

    // Save to settings
    QSettings settings;
    settings.setValue("google/apiKey", m_apiKey);

    if (!m_apiKey.isEmpty()) {
        setStatusMessage("API key configured");
        qDebug() << "GoogleSpeechRecognizer: API key set";
    } else {
        setStatusMessage("API key cleared");
        qDebug() << "GoogleSpeechRecognizer: API key cleared";
    }
}

void GoogleSpeechRecognizer::setLanguageCode(const QString &languageCode)
{
    m_languageCode = languageCode;
    qDebug() << "GoogleSpeechRecognizer: Language code set to" << m_languageCode;
}

void GoogleSpeechRecognizer::setMaxListeningDuration(int maxDuration)
{
    m_maxListeningDuration = qBound(1000, maxDuration, 60000);
    qDebug() << "GoogleSpeechRecognizer: Max listening duration set to" << m_maxListeningDuration << "ms";
}

// ========================================================================
// RECORDING CONTROL
// ========================================================================

void GoogleSpeechRecognizer::startListening()
{
    if (m_apiKey.isEmpty()) {
        emit error("API key not configured");
        setStatusMessage("Error: No API key");
        return;
    }

    if (m_isListening) {
        qWarning() << "GoogleSpeechRecognizer: Already listening";
        return;
    }

    qDebug() << "GoogleSpeechRecognizer: Starting to listen...";

    // Initialize audio
    if (!initializeAudio()) {
        emit error("Failed to initialize audio");
        setStatusMessage("Error: Audio initialization failed");
        return;
    }

    m_isListening = true;
    emit listeningChanged();
    setStatusMessage("Listening...");
    emit recordingStarted();

    // Start audio recording
    m_audioBuffer.clear();
    m_audioSource->start();

    // Start max duration timer
    m_maxDurationTimer->start(m_maxListeningDuration);

    qDebug() << "GoogleSpeechRecognizer: Recording started";
}

void GoogleSpeechRecognizer::stopListening()
{
    if (!m_isListening) {
        qWarning() << "GoogleSpeechRecognizer: Not currently listening";
        return;
    }

    qDebug() << "GoogleSpeechRecognizer: Stopping listening...";

    // Stop audio recording
    if (m_audioSource) {
        m_audioSource->stop();
    }

    m_isListening = false;
    emit listeningChanged();
    emit recordingStopped();

    // Stop timers
    m_silenceTimer->stop();
    m_maxDurationTimer->stop();

    // Check if we have enough audio data
    if (m_audioBuffer.size() < 1000) {
        qWarning() << "GoogleSpeechRecognizer: Insufficient audio data";
        setStatusMessage("Error: No speech detected");
        emit error("No speech detected");
        reset();
        return;
    }

    // Send to Google for recognition
    sendToGoogle();
}

void GoogleSpeechRecognizer::cancel()
{
    qDebug() << "GoogleSpeechRecognizer: Cancelling...";

    if (m_audioSource) {
        m_audioSource->stop();
    }

    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_silenceTimer->stop();
    m_maxDurationTimer->stop();

    reset();
    setStatusMessage("Cancelled");
}

// ========================================================================
// AUDIO HANDLING
// ========================================================================

bool GoogleSpeechRecognizer::initializeAudio()
{
    // Setup audio format (LINEAR16, 16kHz mono - required by Google)
    m_audioFormat.setSampleRate(SAMPLE_RATE);
    m_audioFormat.setChannelCount(CHANNEL_COUNT);
    m_audioFormat.setSampleFormat(QAudioFormat::Int16);

    // Get default audio input device
    QAudioDevice audioDevice = QMediaDevices::defaultAudioInput();
    if (audioDevice.isNull()) {
        qWarning() << "GoogleSpeechRecognizer: No audio input device found";
        return false;
    }

    // Check if format is supported
    if (!audioDevice.isFormatSupported(m_audioFormat)) {
        qWarning() << "GoogleSpeechRecognizer: Audio format not supported";
        qWarning() << "Requested format:" << m_audioFormat;
        return false;
    }

    // Create audio source
    m_audioSource = new QAudioSource(audioDevice, m_audioFormat, this);

    // Create IO device for reading audio data
    m_audioDevice = m_audioSource->start();
    if (!m_audioDevice) {
        qWarning() << "GoogleSpeechRecognizer: Failed to start audio source";
        delete m_audioSource;
        m_audioSource = nullptr;
        return false;
    }

    // Connect to readyRead signal
    connect(m_audioDevice, &QIODevice::readyRead,
            this, &GoogleSpeechRecognizer::onAudioDataReady);

    qDebug() << "GoogleSpeechRecognizer: Audio initialized successfully";
    qDebug() << "  Sample rate:" << m_audioFormat.sampleRate();
    qDebug() << "  Channels:" << m_audioFormat.channelCount();
    qDebug() << "  Sample format:" << m_audioFormat.sampleFormat();

    return true;
}

void GoogleSpeechRecognizer::onAudioDataReady()
{
    if (!m_audioDevice || !m_isListening) {
        return;
    }

    // Read available audio data
    QByteArray audioData = m_audioDevice->readAll();
    if (audioData.isEmpty()) {
        return;
    }

    m_audioBuffer.append(audioData);
    m_lastAudioTime = QDateTime::currentMSecsSinceEpoch();

    // Restart silence timer
    m_silenceTimer->start();

    // Log progress
    qDebug() << "GoogleSpeechRecognizer: Captured" << audioData.size()
             << "bytes (total:" << m_audioBuffer.size() << "bytes)";
}

void GoogleSpeechRecognizer::onSilenceTimeout()
{
    qDebug() << "GoogleSpeechRecognizer: Silence detected, stopping...";
    stopListening();
}

void GoogleSpeechRecognizer::onMaxDurationTimeout()
{
    qDebug() << "GoogleSpeechRecognizer: Max duration reached, stopping...";
    stopListening();
}

// ========================================================================
// GOOGLE SPEECH API
// ========================================================================

void GoogleSpeechRecognizer::sendToGoogle()
{
    if (m_audioBuffer.isEmpty()) {
        qWarning() << "GoogleSpeechRecognizer: No audio data to send";
        return;
    }

    m_isProcessing = true;
    emit processingChanged();
    setStatusMessage("Processing speech...");

    qDebug() << "GoogleSpeechRecognizer: Sending" << m_audioBuffer.size()
             << "bytes to Google Speech API";

    // Build JSON request
    QJsonObject config;
    config["encoding"] = "LINEAR16";
    config["sampleRateHertz"] = SAMPLE_RATE;
    config["languageCode"] = m_languageCode;
    config["enableAutomaticPunctuation"] = true;

    QJsonObject audio;
    audio["content"] = audioToBase64();

    QJsonObject request;
    request["config"] = config;
    request["audio"] = audio;

    // Create network request
    QString urlWithKey = QString("%1?key=%2").arg(API_ENDPOINT, m_apiKey);
    QUrl url(urlWithKey);
    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Send request
    QByteArray requestData = QJsonDocument(request).toJson(QJsonDocument::Compact);
    qDebug() << "GoogleSpeechRecognizer: Request size:" << requestData.size() << "bytes";

    m_currentReply = m_networkManager->post(networkRequest, requestData);

    // Connect reply signals
    connect(m_currentReply, &QNetworkReply::errorOccurred,
            this, &GoogleSpeechRecognizer::onNetworkError);
}

void GoogleSpeechRecognizer::onNetworkReply(QNetworkReply *reply)
{
    if (reply != m_currentReply) {
        return;
    }

    m_isProcessing = false;
    emit processingChanged();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();

    qDebug() << "GoogleSpeechRecognizer: HTTP Status Code:" << statusCode;
    qDebug() << "GoogleSpeechRecognizer: Response size:" << responseData.size() << "bytes";

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        qWarning() << "GoogleSpeechRecognizer: Network error:" << errorMsg;
        qWarning() << "GoogleSpeechRecognizer: Response body:" << responseData;
        emit error("Recognition error: " + errorMsg);
        setStatusMessage("Error: " + errorMsg);
        reply->deleteLater();
        m_currentReply = nullptr;
        reset();
        return;
    }

    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "GoogleSpeechRecognizer: Invalid JSON response";
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
        qWarning() << "GoogleSpeechRecognizer: API error:" << errorMessage;
        emit error("Google API error: " + errorMessage);
        setStatusMessage("Error: " + errorMessage);
        reply->deleteLater();
        m_currentReply = nullptr;
        reset();
        return;
    }

    // Extract transcription
    if (response.contains("results")) {
        QJsonArray results = response["results"].toArray();
        if (!results.isEmpty()) {
            QJsonObject firstResult = results[0].toObject();
            QJsonArray alternatives = firstResult["alternatives"].toArray();

            if (!alternatives.isEmpty()) {
                QJsonObject firstAlternative = alternatives[0].toObject();
                QString transcript = firstAlternative["transcript"].toString();
                double confidence = firstAlternative["confidence"].toDouble();

                qDebug() << "GoogleSpeechRecognizer: Transcript:" << transcript;
                qDebug() << "GoogleSpeechRecognizer: Confidence:" << confidence;

                emit transcriptionReceived(transcript, true);
                setStatusMessage("Transcription complete");
            } else {
                qWarning() << "GoogleSpeechRecognizer: No alternatives in response";
                emit error("No speech recognized");
                setStatusMessage("No speech recognized");
            }
        } else {
            qWarning() << "GoogleSpeechRecognizer: No results in response";
            emit error("No speech recognized");
            setStatusMessage("No speech recognized");
        }
    } else {
        qWarning() << "GoogleSpeechRecognizer: No results field in response";
        emit error("No speech recognized");
        setStatusMessage("No speech recognized");
    }

    reply->deleteLater();
    m_currentReply = nullptr;
    reset();
}

void GoogleSpeechRecognizer::onNetworkError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);

    if (!m_currentReply) {
        return;
    }

    QString errorMsg = m_currentReply->errorString();
    qWarning() << "GoogleSpeechRecognizer: Network error:" << errorMsg;

    m_isProcessing = false;
    emit processingChanged();
    emit this->error("Connection error: " + errorMsg);
    setStatusMessage("Error: " + errorMsg);

    reset();
}

// ========================================================================
// HELPER METHODS
// ========================================================================

QString GoogleSpeechRecognizer::audioToBase64() const
{
    return QString::fromLatin1(m_audioBuffer.toBase64());
}

void GoogleSpeechRecognizer::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "GoogleSpeechRecognizer:" << msg;
    }
}

void GoogleSpeechRecognizer::reset()
{
    m_audioBuffer.clear();
    m_lastAudioTime = 0;

    if (m_audioSource) {
        delete m_audioSource;
        m_audioSource = nullptr;
        m_audioDevice = nullptr;
    }

    m_isListening = false;
    m_isProcessing = false;
    emit listeningChanged();
    emit processingChanged();
}
