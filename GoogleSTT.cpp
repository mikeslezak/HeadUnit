#include "GoogleSTT.h"
#include <QDebug>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

GoogleSTT::GoogleSTT(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_languageCode("en-US")
    , m_isProcessing(false)
    , m_statusMessage("Not configured")
{
    qDebug() << "GoogleSTT: Initializing...";

    // Load API key from environment first, fall back to QSettings
    m_apiKey = qEnvironmentVariable("GOOGLE_API_KEY");
    if (m_apiKey.isEmpty()) {
        QSettings settings;
        m_apiKey = settings.value("google/apiKey", "").toString();
    }

    if (!m_apiKey.isEmpty()) {
        setStatusMessage("Ready - API key configured");
        qDebug() << "GoogleSTT: API key loaded";
    } else {
        setStatusMessage("API key required");
        qDebug() << "GoogleSTT: No API key configured";
    }

    // Connect network manager signals
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &GoogleSTT::onNetworkReply);

    qDebug() << "GoogleSTT: Initialization complete";
}

GoogleSTT::~GoogleSTT()
{
    // In the destructor, the event loop won't run again so we must
    // delete the reply directly (deleteLater won't execute).
    if (m_currentReply) {
        m_currentReply->abort();
        delete m_currentReply;
        m_currentReply = nullptr;
    }
}

// ========================================================================
// CONFIGURATION
// ========================================================================

void GoogleSTT::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();

    if (!m_apiKey.isEmpty()) {
        setStatusMessage("API key configured");
        qDebug() << "GoogleSTT: API key set";
    } else {
        setStatusMessage("API key cleared");
        qDebug() << "GoogleSTT: API key cleared";
    }
}

void GoogleSTT::setLanguageCode(const QString &languageCode)
{
    m_languageCode = languageCode;
    qDebug() << "GoogleSTT: Language code set to" << m_languageCode;
}

void GoogleSTT::setSpeechContextHints(const QStringList &phrases)
{
    m_speechContextHints = phrases;
    qDebug() << "GoogleSTT: Speech context hints set:" << phrases.size() << "phrases";
}

// ========================================================================
// TRANSCRIPTION
// ========================================================================

void GoogleSTT::transcribe(const QVector<int16_t> &samples)
{
    // Convert int16_t samples to QByteArray
    QByteArray audioData;
    audioData.resize(samples.size() * sizeof(int16_t));
    memcpy(audioData.data(), samples.constData(), audioData.size());

    transcribeRaw(audioData);
}

void GoogleSTT::transcribeRaw(const QByteArray &audioData)
{
    if (m_apiKey.isEmpty()) {
        emit error("API key not configured");
        setStatusMessage("Error: No API key");
        return;
    }

    if (audioData.isEmpty()) {
        emit error("Empty audio data");
        return;
    }

    if (m_isProcessing) {
        qWarning() << "GoogleSTT: Already processing a request, dropping new one";
        emit error("STT busy — already processing");
        return;
    }

    m_isProcessing = true;
    emit processingChanged();
    setStatusMessage("Sending to Google STT...");

    sendToGoogle(audioData);
}

void GoogleSTT::cancel()
{
    if (m_currentReply) {
        // Don't deleteLater here — the QNetworkAccessManager::finished signal will
        // still fire for the aborted reply, and onNetworkReply handles deletion.
        m_currentReply->abort();
        m_currentReply = nullptr;
    }

    m_isProcessing = false;
    emit processingChanged();
    setStatusMessage("Request cancelled");
}

// ========================================================================
// NETWORK HANDLING
// ========================================================================

void GoogleSTT::sendToGoogle(const QByteArray &audioData)
{
    // Encode audio to base64
    QString audioContent = encodeAudioToBase64(audioData);

    // Build request JSON
    QJsonObject config;
    config["encoding"] = "LINEAR16";
    config["sampleRateHertz"] = SAMPLE_RATE;
    config["languageCode"] = m_languageCode;
    config["enableAutomaticPunctuation"] = true;
    config["model"] = "latest_short";  // Optimized for short voice commands
    config["useEnhanced"] = true;       // Enhanced model — better noise handling

    // Vehicle-specific metadata for optimized recognition
    QJsonObject metadata;
    metadata["interactionType"] = "VOICE_COMMAND";
    metadata["microphoneDistance"] = "NEARFIELD";
    metadata["recordingDeviceType"] = "VEHICLE";
    config["metadata"] = metadata;

    // Add speech context hints if available (up to 500 phrases)
    // Google STT expects phrases as an array of strings, not objects
    if (!m_speechContextHints.isEmpty()) {
        QJsonArray phrases;
        int maxHints = qMin(m_speechContextHints.size(), 500);
        for (int i = 0; i < maxHints; ++i) {
            phrases.append(m_speechContextHints[i]);  // Direct string, not object
        }

        QJsonObject speechContext;
        speechContext["phrases"] = phrases;
        speechContext["boost"] = 20;  // Boost recognition of these phrases

        QJsonArray speechContexts;
        speechContexts.append(speechContext);
        config["speechContexts"] = speechContexts;

        qDebug() << "GoogleSTT: Using" << maxHints << "speech context hints";
    }

    QJsonObject audio;
    audio["content"] = audioContent;

    QJsonObject request;
    request["config"] = config;
    request["audio"] = audio;

    // Create network request
    QString url = QString("%1?key=%2").arg(API_ENDPOINT, m_apiKey);
    QUrl requestUrl(url);
    QNetworkRequest networkRequest{requestUrl};
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setTransferTimeout(15000);

    // Send request
    QByteArray requestData = QJsonDocument(request).toJson(QJsonDocument::Compact);
    qDebug() << "GoogleSTT: Sending request, audio size:" << audioData.size() << "bytes";

    m_currentReply = m_networkManager->post(networkRequest, requestData);

    // Connect error signal
    connect(m_currentReply, &QNetworkReply::errorOccurred,
            this, &GoogleSTT::onNetworkError);
}

void GoogleSTT::onNetworkReply(QNetworkReply *reply)
{
    if (reply != m_currentReply) {
        reply->deleteLater();
        return;
    }

    m_isProcessing = false;
    emit processingChanged();

    QByteArray responseData = reply->readAll();

    qDebug() << "GoogleSTT: HTTP Status Code:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        qWarning() << "GoogleSTT: Network error:" << errorMsg;
        qWarning() << "GoogleSTT: Response body:" << responseData;
        emit error("Network error: " + errorMsg);
        setStatusMessage("Error: " + errorMsg);
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        emit error("Invalid JSON response");
        setStatusMessage("Error: Invalid response");
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    QJsonObject json = doc.object();

    // Check for API errors
    if (json.contains("error")) {
        QJsonObject errorObj = json["error"].toObject();
        QString errorMessage = errorObj["message"].toString();
        qWarning() << "GoogleSTT: API error:" << errorMessage;
        emit error("API error: " + errorMessage);
        setStatusMessage("Error: " + errorMessage);
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    // Extract transcription results
    QString transcription;
    float confidence = 0.0f;

    if (json.contains("results")) {
        QJsonArray results = json["results"].toArray();
        if (!results.isEmpty()) {
            QJsonObject firstResult = results[0].toObject();
            QJsonArray alternatives = firstResult["alternatives"].toArray();
            if (!alternatives.isEmpty()) {
                QJsonObject firstAlternative = alternatives[0].toObject();
                transcription = firstAlternative["transcript"].toString();
                confidence = firstAlternative["confidence"].toDouble();
            }
        }
    }

    if (transcription.isEmpty()) {
        qDebug() << "GoogleSTT: No transcription result (silence or unclear audio)";
        setStatusMessage("No speech detected");
        emit transcriptionReady("", 0.0f);
    } else {
        qDebug() << "GoogleSTT: Transcription:" << transcription << "confidence:" << confidence;
        setStatusMessage("Transcription complete");
        emit transcriptionReady(transcription, confidence);
    }

    reply->deleteLater();
    m_currentReply = nullptr;
}

void GoogleSTT::onNetworkError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);

    if (!m_currentReply) {
        return;
    }

    // Log the error here but do NOT delete/null m_currentReply.
    // The QNetworkAccessManager::finished signal always fires after errorOccurred,
    // and onNetworkReply handles cleanup. Deleting here causes double-deletion.
    qWarning() << "GoogleSTT: Network error:" << m_currentReply->errorString();
}

// ========================================================================
// HELPER METHODS
// ========================================================================

void GoogleSTT::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "GoogleSTT:" << msg;
    }
}

QString GoogleSTT::encodeAudioToBase64(const QByteArray &audioData)
{
    return QString::fromLatin1(audioData.toBase64());
}
