#include "WakeWordDetector.h"
#include <QDebug>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QFile>
#include <QDir>
#include <QAudioSource>
#include <QDateTime>

// Porcupine C API
extern "C" {
#include "pv_porcupine.h"
}

WakeWordDetector::WakeWordDetector(QObject *parent)
    : QObject(parent)
    , m_porcupine(nullptr)
    , m_isInitialized(false)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_sensitivity(0.5f)  // Default: balanced sensitivity
    , m_wakeWord("jarvis")  // Default wake word (built-in)
    , m_audioSource(nullptr)
    , m_audioDevice(nullptr)
    , m_frameLength(0)
    , m_sampleRate(0)
    , m_lastDetectionTime(0)  // Initialize debounce timestamp
{
    qDebug() << "WakeWordDetector: Initializing...";
    setStatusMessage("Initializing wake word detection");
}

WakeWordDetector::~WakeWordDetector()
{
    stop();
    cleanupPorcupine();
}

// ========== CONTROL METHODS ==========

void WakeWordDetector::start()
{
    QMutexLocker locker(&m_mutex);

    if (m_isRunning) {
        qDebug() << "WakeWordDetector: Already running";
        return;
    }

    // Initialize Porcupine if not already done
    if (!m_isInitialized) {
        if (!initializePorcupine()) {
            emit error("Failed to initialize Porcupine wake word engine");
            return;
        }
    }

    // Setup audio input
    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(1);  // Mono
    format.setSampleFormat(QAudioFormat::Int16);  // 16-bit PCM

    // Get default audio input device
    QAudioDevice deviceInfo = QMediaDevices::defaultAudioInput();
    if (deviceInfo.isNull()) {
        emit error("No audio input device found");
        setStatusMessage("Error: No microphone found");
        return;
    }

    // Check if format is supported
    if (!deviceInfo.isFormatSupported(format)) {
        qWarning() << "WakeWordDetector: Default format not supported, trying to use nearest";
        // Qt will try to use the nearest supported format
    }

    m_audioSource = new QAudioSource(deviceInfo, format, this);
    m_audioDevice = m_audioSource->start();

    if (!m_audioDevice) {
        emit error("Failed to start audio input");
        setStatusMessage("Error: Could not start microphone");
        delete m_audioSource;
        m_audioSource = nullptr;
        return;
    }

    // Connect to audio ready signal
    connect(m_audioDevice, &QIODevice::readyRead, this, &WakeWordDetector::onAudioReady);

    m_isRunning = true;
    m_isPaused = false;
    emit runningChanged();

    setStatusMessage(QString("Listening for '%1'...").arg(m_wakeWord));
    qDebug() << "WakeWordDetector: Started listening for" << m_wakeWord;
}

void WakeWordDetector::stop()
{
    QMutexLocker locker(&m_mutex);

    if (!m_isRunning) {
        return;
    }

    // Stop audio input
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_audioDevice = nullptr;  // Deleted by QAudioSource
    }

    m_isRunning = false;
    m_isPaused = false;
    emit runningChanged();

    setStatusMessage("Wake word detection stopped");
    qDebug() << "WakeWordDetector: Stopped";
}

void WakeWordDetector::pause()
{
    if (!m_isRunning || m_isPaused) {
        return;
    }

    m_isPaused = true;
    setStatusMessage("Wake word detection paused");
    qDebug() << "WakeWordDetector: Paused";
}

void WakeWordDetector::resume()
{
    if (!m_isRunning || !m_isPaused) {
        return;
    }

    m_isPaused = false;
    setStatusMessage(QString("Listening for '%1'...").arg(m_wakeWord));
    qDebug() << "WakeWordDetector: Resumed";
}

// ========== SETTINGS ==========

void WakeWordDetector::setSensitivity(float sensitivity)
{
    if (sensitivity < 0.0f) sensitivity = 0.0f;
    if (sensitivity > 1.0f) sensitivity = 1.0f;

    if (qFuzzyCompare(m_sensitivity, sensitivity)) {
        return;
    }

    m_sensitivity = sensitivity;
    emit sensitivityChanged();

    // Reinitialize Porcupine if running
    if (m_isInitialized) {
        bool wasRunning = m_isRunning;
        if (wasRunning) {
            stop();
        }

        cleanupPorcupine();
        initializePorcupine();

        if (wasRunning) {
            start();
        }
    }

    qDebug() << "WakeWordDetector: Sensitivity changed to" << m_sensitivity;
}

void WakeWordDetector::setWakeWord(const QString &keyword)
{
    if (m_wakeWord == keyword) {
        return;
    }

    m_wakeWord = keyword;
    emit wakeWordChanged();

    // Reinitialize Porcupine with new keyword
    if (m_isInitialized) {
        bool wasRunning = m_isRunning;
        if (wasRunning) {
            stop();
        }

        cleanupPorcupine();
        initializePorcupine();

        if (wasRunning) {
            start();
        }
    }

    qDebug() << "WakeWordDetector: Wake word changed to" << m_wakeWord;
}

void WakeWordDetector::setAccessKey(const QString &accessKey)
{
    m_accessKey = accessKey;
    qDebug() << "WakeWordDetector: Access key set";
}

// ========== AUDIO PROCESSING ==========

void WakeWordDetector::onAudioReady()
{
    if (!m_audioDevice || m_isPaused) {
        return;
    }

    // Read available audio data
    QByteArray data = m_audioDevice->readAll();
    m_audioBuffer.append(data);

    // Process complete frames
    const int bytesPerFrame = m_frameLength * sizeof(int16_t);

    while (m_audioBuffer.size() >= bytesPerFrame) {
        const int16_t *audioFrame = reinterpret_cast<const int16_t*>(m_audioBuffer.constData());

        // Process this frame
        int32_t keywordIndex = processAudioFrame(audioFrame);

        if (keywordIndex >= 0) {
            // Wake word detected! Check debouncing
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            qint64 timeSinceLastDetection = now - m_lastDetectionTime;

            // Debounce: Ignore detections within 1000ms of the last one
            if (timeSinceLastDetection >= 1000 || m_lastDetectionTime == 0) {
                qDebug() << "WakeWordDetector: Wake word detected!" << m_wakeWord;
                m_lastDetectionTime = now;  // Update timestamp
                emit wakeWordDetected(m_wakeWord, keywordIndex);
            } else {
                qDebug() << "WakeWordDetector: Ignoring duplicate detection (" << timeSinceLastDetection << "ms since last)";
            }
        }

        // Remove processed frame from buffer
        m_audioBuffer.remove(0, bytesPerFrame);
    }
}

int32_t WakeWordDetector::processAudioFrame(const int16_t *audioFrame)
{
    if (!m_porcupine) {
        return -1;
    }

    int32_t keywordIndex = -1;
    pv_status_t status = pv_porcupine_process(m_porcupine, audioFrame, &keywordIndex);

    if (status != PV_STATUS_SUCCESS) {
        qWarning() << "WakeWordDetector: Porcupine process error:" << pv_status_to_string(status);
        return -1;
    }

    return keywordIndex;
}

// ========== PORCUPINE INITIALIZATION ==========

bool WakeWordDetector::initializePorcupine()
{
    if (m_isInitialized) {
        qWarning() << "WakeWordDetector: Already initialized";
        return true;
    }

    // Check for access key
    if (m_accessKey.isEmpty()) {
        qWarning() << "WakeWordDetector: No access key provided. Using default (may have limitations).";
        // Porcupine may work with empty key for testing, but will have limits
    }

    // Get model and keyword paths
    QString modelPath = getModelPath();
    QString keywordPath = getKeywordPath();

    qDebug() << "WakeWordDetector: Model path:" << modelPath;
    qDebug() << "WakeWordDetector: Keyword path:" << keywordPath;

    // Verify files exist
    if (!QFile::exists(modelPath)) {
        qCritical() << "WakeWordDetector: Model file not found:" << modelPath;
        setStatusMessage("Error: Model file not found");
        return false;
    }

    if (!QFile::exists(keywordPath)) {
        qCritical() << "WakeWordDetector: Keyword file not found:" << keywordPath;
        setStatusMessage("Error: Keyword file not found");
        return false;
    }

    // Prepare parameters for Porcupine
    const char *accessKey = m_accessKey.isEmpty() ? nullptr : m_accessKey.toUtf8().constData();
    const char *modelPathC = modelPath.toUtf8().constData();
    const char *keywordPathC = keywordPath.toUtf8().constData();
    const char *keywordPaths[] = { keywordPathC };
    const float sensitivities[] = { m_sensitivity };

    // Initialize Porcupine
    pv_status_t status = pv_porcupine_init(
        accessKey,
        modelPathC,
        1,  // num_keywords
        keywordPaths,
        sensitivities,
        &m_porcupine
    );

    if (status != PV_STATUS_SUCCESS) {
        qCritical() << "WakeWordDetector: Failed to initialize Porcupine:" << pv_status_to_string(status);

        // Get detailed error messages
        char **messageStack = nullptr;
        int32_t messageStackDepth = 0;
        pv_status_t errorStatus = pv_get_error_stack(&messageStack, &messageStackDepth);

        if (errorStatus == PV_STATUS_SUCCESS) {
            for (int32_t i = 0; i < messageStackDepth; i++) {
                qCritical() << "  -" << messageStack[i];
            }
            pv_free_error_stack(messageStack);
        }

        setStatusMessage("Error: Failed to initialize Porcupine");
        return false;
    }

    // Get audio parameters from Porcupine
    m_frameLength = pv_porcupine_frame_length();
    m_sampleRate = pv_sample_rate();

    qDebug() << "WakeWordDetector: Porcupine initialized successfully";
    qDebug() << "  Frame length:" << m_frameLength << "samples";
    qDebug() << "  Sample rate:" << m_sampleRate << "Hz";
    qDebug() << "  Version:" << pv_porcupine_version();

    m_isInitialized = true;
    emit initializedChanged();
    setStatusMessage("Wake word engine initialized");

    return true;
}

void WakeWordDetector::cleanupPorcupine()
{
    if (m_porcupine) {
        pv_porcupine_delete(m_porcupine);
        m_porcupine = nullptr;
    }

    m_isInitialized = false;
    emit initializedChanged();
}

// ========== PATH HELPERS ==========

QString WakeWordDetector::getModelPath() const
{
    // Path to Porcupine model file (English)
    // Use source directory path instead of current working directory (build dir)
    return "/home/mike/HeadUnit/external/porcupine/lib/common/porcupine_params.pv";
}

QString WakeWordDetector::getKeywordPath() const
{
    // Map wake word name to filename
    // Use Raspberry Pi keyword files for ARM platform (Jetson Orin Nano)
    QString keywordName = m_wakeWord;
    keywordName.replace("_", " ");  // e.g., "hey_google" -> "hey google"

    // Use source directory path instead of current working directory (build dir)
    return QString("/home/mike/HeadUnit/external/porcupine/resources/keyword_files/raspberry-pi/%1_raspberry-pi.ppn")
        .arg(keywordName);
}

// ========== HELPER METHODS ==========

void WakeWordDetector::setStatusMessage(const QString &msg)
{
    if (m_statusMessage == msg) {
        return;
    }

    m_statusMessage = msg;
    emit statusMessageChanged();
}
