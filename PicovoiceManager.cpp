#include "PicovoiceManager.h"
#include "GoogleSTT.h"
#include <QDebug>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QtMath>
#include <QCoreApplication>
#include <QTimer>

// Picovoice C API
extern "C" {
#include "pv_porcupine.h"
#include "pv_rhino.h"
#include "pv_leopard.h"
#include "pv_koala.h"
}

PicovoiceManager::PicovoiceManager(QObject *parent)
    : QObject(parent)
    , m_state(Listening)
    , m_porcupine(nullptr)
    , m_rhino(nullptr)
    , m_leopard(nullptr)
    , m_koala(nullptr)
    , m_googleSTT(nullptr)
    , m_wakeWord("jarvis")
    , m_sensitivity(0.5f)
    , m_audioSource(nullptr)
    , m_audioDevice(nullptr)
    , m_frameLength(0)
    , m_sampleRate(0)
    , m_koalaFrameLength(0)
    , m_koalaDelaySamples(0)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_isInitialized(false)
    , m_wakeWordAvailable(false)
    , m_speechStartTime(0)
    , m_lastVoiceActivityTime(0)
    , m_lastDetectionTime(0)
    , m_followUpTimer(nullptr)
    , m_readyPromptTimer(nullptr)
    , m_speechStartTimer(nullptr)
    , m_transcriptionTimer(nullptr)
    , m_commandTimer(nullptr)
{
    qDebug() << "PicovoiceManager: Initializing unified voice pipeline...";
    setStatusMessage("Initializing Picovoice");

    // Create Google STT instance
    m_googleSTT = new GoogleSTT(this);
    connect(m_googleSTT, &GoogleSTT::transcriptionReady,
            this, &PicovoiceManager::onGoogleTranscriptionReady);
    connect(m_googleSTT, &GoogleSTT::error,
            this, &PicovoiceManager::onGoogleError);

    // Follow-up timer: returns to Listening after 12s silence in follow-up mode
    m_followUpTimer = new QTimer(this);
    m_followUpTimer->setSingleShot(true);
    m_followUpTimer->setInterval(FOLLOW_UP_TIMEOUT_MS);
    connect(m_followUpTimer, &QTimer::timeout, this, [this]() {
        qDebug() << "PicovoiceManager: Follow-up timeout, returning to listening";
        resetToListening();
        emit interactionReset();
    });

    // Ready prompt safety timer: recovers if TTS fails during ready prompt
    m_readyPromptTimer = new QTimer(this);
    m_readyPromptTimer->setSingleShot(true);
    m_readyPromptTimer->setInterval(5000);
    connect(m_readyPromptTimer, &QTimer::timeout, this, [this]() {
        if (m_state == WaitingForReadyPrompt) {
            qWarning() << "PicovoiceManager: Ready prompt timeout — TTS may have failed, resetting";
            cancelAndReset();
        }
    });

    // Speech start timeout — if user doesn't start speaking within 6s after ready prompt, reset
    m_speechStartTimer = new QTimer(this);
    m_speechStartTimer->setSingleShot(true);
    m_speechStartTimer->setInterval(8000);
    connect(m_speechStartTimer, &QTimer::timeout, this, [this]() {
        if (m_state == WaitingForSpeechStart) {
            qDebug() << "PicovoiceManager: No speech detected within 8s, resetting to listening";
            resetToListening();
            emit interactionReset();
        }
    });

    // Rhino command timeout — if Rhino never finalizes within 10s, fall back to STT
    m_commandTimer = new QTimer(this);
    m_commandTimer->setSingleShot(true);
    m_commandTimer->setInterval(10000);
    connect(m_commandTimer, &QTimer::timeout, this, [this]() {
        if (m_state == WaitingForCommand) {
            qWarning() << "PicovoiceManager: Rhino command timeout (10s), falling back to STT";
            if (m_rhino) pv_rhino_reset(m_rhino);
            // Use accumulated speech buffer for STT
            if (!m_speechBuffer.isEmpty()) {
                finalizeLeopardTranscription();
            } else {
                resetToListening();
                emit interactionReset();
            }
        }
    });

    // Transcription timeout — if Google STT doesn't respond within 15s, reset
    m_transcriptionTimer = new QTimer(this);
    m_transcriptionTimer->setSingleShot(true);
    m_transcriptionTimer->setInterval(15000);
    connect(m_transcriptionTimer, &QTimer::timeout, this, [this]() {
        if (m_state == WaitingForTranscription) {
            qWarning() << "PicovoiceManager: Transcription timeout (15s), resetting";
            if (m_googleSTT) m_googleSTT->cancel();
            resetToListening();
            emit interactionReset();
        }
    });
}

PicovoiceManager::~PicovoiceManager()
{
    stop();
    cleanup();
}

// ========== CONTROL METHODS ==========

void PicovoiceManager::start()
{
    if (m_isRunning) {
        qDebug() << "PicovoiceManager: Already running";
        return;
    }

    // Initialize all Picovoice components if not already done
    if (!m_isInitialized) {
        qDebug() << "PicovoiceManager: Initializing components...";

        // Koala is optional - noise suppression improves quality but isn't required
        if (!initializeKoala()) {
            qWarning() << "PicovoiceManager: Koala initialization failed. Continuing without noise suppression.";
            // Not a fatal error - wake word and STT can still work
        }

        if (!initializePorcupine()) {
            qWarning() << "PicovoiceManager: Porcupine init failed. Wake word detection disabled - use button activation.";
            m_wakeWordAvailable = false;
        } else {
            m_wakeWordAvailable = true;
        }
        emit wakeWordAvailableChanged();

        if (!initializeRhino()) {
            qWarning() << "PicovoiceManager: Rhino initialization failed (context file may be missing). Continuing without intent detection.";
        }

        if (!initializeLeopard()) {
            qWarning() << "PicovoiceManager: Leopard init failed. Will use Google STT only.";
        }

        m_isInitialized = true;
        emit initializedChanged();
    }

    // Use Picovoice sample rate if available, otherwise default 16kHz
    if (m_sampleRate == 0) {
        m_sampleRate = 16000;
    }
    if (m_frameLength == 0) {
        m_frameLength = 512;
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
        qWarning() << "PicovoiceManager: Default format not supported, trying to use nearest";
    }

    m_audioSource = new QAudioSource(deviceInfo, format, this);
    m_audioSource->setBufferSize(4096);  // 128ms at 16kHz mono 16-bit — low latency for voice
    m_audioDevice = m_audioSource->start();

    if (!m_audioDevice) {
        emit error("Failed to start audio input");
        setStatusMessage("Error: Could not start microphone");
        delete m_audioSource;
        m_audioSource = nullptr;
        return;
    }

    // Connect to audio ready signal
    connect(m_audioDevice, &QIODevice::readyRead, this, &PicovoiceManager::onAudioReady);

    // Monitor audio source for errors and auto-recover (with retry limit)
    connect(m_audioSource, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::StoppedState && m_audioSource->error() != QAudio::NoError) {
            m_audioRecoveryAttempts++;
            if (m_audioRecoveryAttempts > MAX_AUDIO_RECOVERY_ATTEMPTS) {
                qCritical() << "PicovoiceManager: Audio recovery failed after"
                            << MAX_AUDIO_RECOVERY_ATTEMPTS << "attempts, stopping";
                emit error("Microphone disconnected — restart to retry");
                return;
            }
            qWarning() << "PicovoiceManager: Audio source error:" << m_audioSource->error()
                       << "— restart attempt" << m_audioRecoveryAttempts;
            QTimer::singleShot(500, this, [this]() {
                if (m_audioSource && m_isRunning) {
                    auto *oldDevice = m_audioDevice;
                    m_audioDevice = m_audioSource->start();
                    if (m_audioDevice) {
                        // Always disconnect old device to prevent duplicate connections
                        // (Qt may reuse the same QIODevice pointer on restart)
                        if (oldDevice) {
                            disconnect(oldDevice, &QIODevice::readyRead, this, &PicovoiceManager::onAudioReady);
                        }
                        connect(m_audioDevice, &QIODevice::readyRead, this, &PicovoiceManager::onAudioReady);
                        m_audioRecoveryAttempts = 0;  // Reset on success
                        qDebug() << "PicovoiceManager: Audio capture restarted successfully";
                    } else {
                        qCritical() << "PicovoiceManager: Failed to restart audio capture";
                    }
                }
            });
        }
    });

    m_isRunning = true;
    m_isPaused = false;
    m_state = Listening;
    emit runningChanged();

    if (m_wakeWordAvailable) {
        setStatusMessage(QString("Listening for '%1'...").arg(m_wakeWord));
        qDebug() << "PicovoiceManager: Started - listening for wake word";
    } else {
        setStatusMessage("Ready (button activation only)");
        qDebug() << "PicovoiceManager: Started - button activation mode (no wake word)";
    }
}

void PicovoiceManager::stop()
{
    if (!m_isRunning) {
        return;
    }

    // Stop all timers before tearing down audio
    m_followUpTimer->stop();
    m_readyPromptTimer->stop();
    m_speechStartTimer->stop();
    m_transcriptionTimer->stop();
    m_commandTimer->stop();

    // Stop audio input
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_audioDevice = nullptr;  // Deleted by QAudioSource
    }

    m_isRunning = false;
    m_isPaused = false;
    m_state = Listening;
    emit runningChanged();

    setStatusMessage("Voice pipeline stopped");
    qDebug() << "PicovoiceManager: Stopped";
}

void PicovoiceManager::pause()
{
    if (!m_isRunning || m_isPaused) {
        return;
    }

    m_isPaused = true;

    // Stop ALL active state timers — they'll resume when resume() is called if still relevant.
    // Without this, timers fire during TTS playback and silently reset state.
    m_speechStartTimer->stop();
    m_readyPromptTimer->stop();
    m_commandTimer->stop();
    m_followUpTimer->stop();
    m_transcriptionTimer->stop();

    setStatusMessage("Voice pipeline paused");
    qDebug() << "PicovoiceManager: Paused";
}

void PicovoiceManager::resume()
{
    if (!m_isRunning || !m_isPaused) {
        return;
    }

    m_isPaused = false;
    m_resumeTime = QDateTime::currentMSecsSinceEpoch();

    // Discard any microphone audio captured during the pause (contains TTS echo)
    if (m_audioDevice) m_audioDevice->readAll();  // Flush OS/driver buffer
    m_audioBuffer.clear();

    // Restart the appropriate state timer that was stopped during pause()
    if (m_state == WaitingForReadyPrompt) {
        // Resuming while waiting for ready prompt — restart safety timer
        m_readyPromptTimer->start();
        setStatusMessage("Playing ready prompt...");
        qDebug() << "PicovoiceManager: Resumed into ready prompt wait";
    } else if (m_state == WaitingForFollowUp) {
        // Resuming into follow-up mode — start the timeout now (TTS just finished)
        m_followUpTimer->start();
        setStatusMessage("Listening for follow-up...");
        qDebug() << "PicovoiceManager: Resumed into follow-up mode";
    } else if (m_state == WaitingForSpeechStart) {
        m_speechStartTimer->start();
        setStatusMessage("Listening...");
        qDebug() << "PicovoiceManager: Resumed into speech start detection";
    } else if (m_state == WaitingForCommand) {
        m_commandTimer->start();
        setStatusMessage("Listening for command...");
        qDebug() << "PicovoiceManager: Resumed into command detection";
    } else {
        setStatusMessage(QString("Listening for '%1'...").arg(m_wakeWord));
        qDebug() << "PicovoiceManager: Resumed";
    }
}

// ========== CONFIGURATION ==========

void PicovoiceManager::setAccessKey(const QString &key)
{
    m_accessKey = key;
    qDebug() << "PicovoiceManager: Access key set";
}

void PicovoiceManager::setSensitivity(float sensitivity)
{
    if (sensitivity < 0.0f) sensitivity = 0.0f;
    if (sensitivity > 1.0f) sensitivity = 1.0f;

    if (qFuzzyCompare(m_sensitivity, sensitivity)) {
        return;
    }

    m_sensitivity = sensitivity;
    emit sensitivityChanged();

    // Reinitialize if running
    if (m_isInitialized) {
        bool wasRunning = m_isRunning;
        if (wasRunning) {
            stop();
        }

        cleanup();
        m_isInitialized = false;

        if (wasRunning) {
            start();
        }
    }

    qDebug() << "PicovoiceManager: Sensitivity changed to" << m_sensitivity;
}

void PicovoiceManager::setWakeWord(const QString &keyword)
{
    if (m_wakeWord == keyword) {
        return;
    }

    m_wakeWord = keyword;
    emit wakeWordChanged();

    // Reinitialize if running
    if (m_isInitialized) {
        bool wasRunning = m_isRunning;
        if (wasRunning) {
            stop();
        }

        cleanup();
        m_isInitialized = false;

        if (wasRunning) {
            start();
        }
    }

    qDebug() << "PicovoiceManager: Wake word changed to" << m_wakeWord;
}

void PicovoiceManager::setRhinoContextPath(const QString &path)
{
    m_rhinoContextPath = path;
    qDebug() << "PicovoiceManager: Rhino context path set to" << path;
}

void PicovoiceManager::setGoogleApiKey(const QString &key)
{
    m_googleApiKey = key;
    if (m_googleSTT) {
        m_googleSTT->setApiKey(key);
    }
    qDebug() << "PicovoiceManager: Google API key set";
}

void PicovoiceManager::setSpeechContextHints(const QStringList &hints)
{
    // Build enhanced hints list with command phrases for better recognition
    // Google STT recognizes "Call Ari" better than just "Ari" in context
    QStringList enhancedHints;

    // Add original names
    enhancedHints.append(hints);

    // Add command phrases for short names (4 chars or less are harder to recognize)
    // This helps Google STT understand the context of voice commands
    QStringList commands = {"Call", "Text", "Message"};
    for (const QString &name : hints) {
        // For short names, add command phrases to help recognition
        if (name.length() <= 6) {
            for (const QString &cmd : commands) {
                enhancedHints.append(cmd + " " + name);
            }
        }
    }

    m_speechContextHints = enhancedHints;
    if (m_googleSTT) {
        m_googleSTT->setSpeechContextHints(enhancedHints);
    }
    qDebug() << "PicovoiceManager: Speech context hints set:" << enhancedHints.size()
             << "phrases (from" << hints.size() << "names)";
}

// ========== AUDIO PROCESSING ==========

void PicovoiceManager::onAudioReady()
{
    if (!m_audioDevice) return;

    // ALWAYS read from device to prevent OS/driver buffer buildup.
    // Without this, stale audio (including TTS echo) accumulates during pause
    // and floods the pipeline when we resume.
    QByteArray data = m_audioDevice->readAll();

    if (m_isPaused) return;  // Discard audio while paused

    // Post-resume deaf period — discard audio for a short window after resume
    // to avoid TTS echo/reverberation in ALL states (not just WaitingForFollowUp)
    if (m_resumeTime > 0) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_resumeTime;
        if (elapsed < POST_RESUME_DEAF_MS) return;
        m_resumeTime = 0;  // Clear so we don't check every frame
    }

    m_audioBuffer.append(data);

    // Process Porcupine frames (512 samples per frame)
    // Koala is optional and only applied if initialized
    const int bytesPerFrame = m_frameLength * sizeof(int16_t);

    while (m_audioBuffer.size() >= bytesPerFrame) {
        const int16_t *inputFrame = reinterpret_cast<const int16_t*>(m_audioBuffer.constData());

        // Apply Koala noise suppression if available
        if (m_processedFrame.size() != m_frameLength) {
            m_processedFrame.resize(m_frameLength);
        }
        if (m_koala && m_koalaFrameLength == m_frameLength) {
            // Koala initialized and frame sizes match - apply noise suppression
            pv_status_t status = pv_koala_process(m_koala, inputFrame, m_processedFrame.data());
            if (status != PV_STATUS_SUCCESS) {
                qWarning() << "PicovoiceManager: Koala process error:" << pv_status_to_string(status);
                // Continue without noise suppression
                memcpy(m_processedFrame.data(), inputFrame, bytesPerFrame);
            }
        } else {
            // No Koala or frame size mismatch - use original audio
            memcpy(m_processedFrame.data(), inputFrame, bytesPerFrame);
        }

        // Process frame based on current state
        processAudioFrame(m_processedFrame.data(), m_frameLength);

        // Remove processed frame from buffer
        m_audioBuffer.remove(0, bytesPerFrame);
    }
}

void PicovoiceManager::processAudioFrame(const int16_t *frame, int32_t length)
{
    switch (m_state) {
        case Listening: {
            // Track ambient noise floor while idle (for adaptive speech detection)
            int16_t energy = calculateFrameEnergy(frame, length);
            m_noiseFloor = m_noiseFloor * (1.0f - NOISE_FLOOR_ALPHA) + energy * NOISE_FLOOR_ALPHA;
            // Cap noise floor so speech threshold never exceeds ~6000 RMS
            // (prevents highway wind noise from making speech detection impossible)
            if (m_noiseFloor > 2000.0f) m_noiseFloor = 2000.0f;
            processWakeWord(frame);
            break;
        }

        case WaitingForReadyPrompt:
            // Pre-buffer audio so we capture the user's command even if they start
            // talking before the ready prompt finishes
            for (int32_t i = 0; i < length; ++i) {
                m_speechBuffer.append(frame[i]);
            }
            break;

        case WaitingForCommand:
            processRhinoIntent(frame);
            break;

        case WaitingForSpeechStart: {
            // No Rhino: wait for user to start speaking (energy-based detection)
            // Uses adaptive noise floor — speech must be 3x ambient noise level
            int16_t energy = calculateFrameEnergy(frame, length);
            int16_t adaptiveThreshold = (int16_t)(m_noiseFloor * SPEECH_THRESHOLD_RATIO);
            int16_t threshold = adaptiveThreshold > SILENCE_ENERGY_THRESHOLD ? adaptiveThreshold : SILENCE_ENERGY_THRESHOLD;

            if (energy > threshold) {
                // Speech detected — transition to ProcessingSpeech
                m_speechStartTimer->stop();
                qDebug() << "PicovoiceManager: Speech start detected, recording...";
                m_state = ProcessingSpeech;
                m_speechStartTime = QDateTime::currentMSecsSinceEpoch();
                m_lastVoiceActivityTime = m_speechStartTime;
                m_speechBuffer.clear();
                m_speechBuffer.reserve(16000 * 10);

                // Accumulate this frame
                for (int32_t i = 0; i < length; ++i) {
                    m_speechBuffer.append(frame[i]);
                }
            }
            break;
        }

        case ProcessingSpeech: {
            // Accumulate audio for Leopard
            for (int32_t i = 0; i < length; ++i) {
                m_speechBuffer.append(frame[i]);
            }

            qint64 now = QDateTime::currentMSecsSinceEpoch();
            qint64 speechDuration = now - m_speechStartTime;

            // Calculate frame energy for VAD (adaptive threshold)
            int16_t energy = calculateFrameEnergy(frame, length);
            int16_t adaptiveThreshold = (int16_t)(m_noiseFloor * SPEECH_THRESHOLD_RATIO);
            int16_t threshold = adaptiveThreshold > SILENCE_ENERGY_THRESHOLD ? adaptiveThreshold : SILENCE_ENERGY_THRESHOLD;

            // Track voice activity
            if (energy > threshold) {
                m_lastVoiceActivityTime = now;
            }

            // Check for silence-based finalization (after minimum speech duration)
            if (speechDuration > MIN_SPEECH_DURATION_MS) {
                qint64 silenceDuration = now - m_lastVoiceActivityTime;
                if (silenceDuration > SILENCE_THRESHOLD_MS) {
                    qDebug() << "PicovoiceManager: Silence detected (" << silenceDuration << "ms), finalizing...";
                    finalizeLeopardTranscription();
                    break;
                }
            }

            // Check if we've reached max speech duration (fallback)
            if (speechDuration > MAX_SPEECH_DURATION_MS) {
                qDebug() << "PicovoiceManager: Max speech duration reached, finalizing...";
                finalizeLeopardTranscription();
            }
            break;
        }

        case WaitingForTranscription:
            // Audio sent to STT — nothing to do, just discard frames until result arrives
            break;

        case WaitingForFollowUp: {
            // In follow-up mode: no wake word needed, detect speech directly
            // (Post-resume deaf period is handled globally in onAudioReady)
            int16_t energy = calculateFrameEnergy(frame, length);
            int16_t adaptiveThreshold = (int16_t)(m_noiseFloor * SPEECH_THRESHOLD_RATIO);
            int16_t threshold = adaptiveThreshold > SILENCE_ENERGY_THRESHOLD ? adaptiveThreshold : SILENCE_ENERGY_THRESHOLD;

            if (energy > threshold) {
                // Speech detected — stop the timeout and start accumulating
                m_followUpTimer->stop();
                qDebug() << "PicovoiceManager: Follow-up speech detected, recording...";
                m_state = ProcessingSpeech;
                m_speechStartTime = QDateTime::currentMSecsSinceEpoch();
                m_lastVoiceActivityTime = m_speechStartTime;
                m_speechBuffer.clear();
                m_speechBuffer.reserve(16000 * 10);

                // Accumulate this frame
                for (int32_t i = 0; i < length; ++i) {
                    m_speechBuffer.append(frame[i]);
                }
            }
            break;
        }
    }
}

void PicovoiceManager::processWakeWord(const int16_t *frame)
{
    if (!m_porcupine) {
        return;
    }

    int32_t keywordIndex = -1;
    pv_status_t status = pv_porcupine_process(m_porcupine, frame, &keywordIndex);

    if (status != PV_STATUS_SUCCESS) {
        qWarning() << "PicovoiceManager: Porcupine process error:" << pv_status_to_string(status);
        return;
    }

    if (keywordIndex >= 0) {
        // Wake word detected! Check debouncing
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 timeSinceLastDetection = now - m_lastDetectionTime;

        if (timeSinceLastDetection >= DETECTION_DEBOUNCE_MS || m_lastDetectionTime == 0) {
            qDebug() << "PicovoiceManager: Wake word detected!" << m_wakeWord;
            m_lastDetectionTime = now;

            emit wakeWordDetected(m_wakeWord);

            // Transition to WaitingForReadyPrompt state - wait for TTS prompt to finish
            // Pre-buffer audio so we capture user's command even if they start talking early
            m_state = WaitingForReadyPrompt;
            m_speechBuffer.clear();
            m_speechBuffer.reserve(16000 * 10);
            m_readyPromptTimer->start();
            setStatusMessage("Playing ready prompt...");
            qDebug() << "PicovoiceManager: Waiting for ready prompt to finish...";
        } else {
            qDebug() << "PicovoiceManager: Ignoring duplicate detection (" << timeSinceLastDetection << "ms since last)";
        }
    }
}

void PicovoiceManager::onReadyPromptFinished()
{
    m_readyPromptTimer->stop();

    if (m_state != WaitingForReadyPrompt) {
        qDebug() << "PicovoiceManager: onReadyPromptFinished called but not in WaitingForReadyPrompt state";
        return;
    }

    qDebug() << "PicovoiceManager: Ready prompt finished, now listening for command...";
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Now transition to actual command listening
    if (m_rhino) {
        m_state = WaitingForCommand;
        m_commandTimer->start();
        setStatusMessage("Listening for command...");
    } else {
        // No Rhino, go straight to speech accumulation.
        // Use WaitingForFollowUp-like approach: wait for the user to actually start
        // speaking before starting ProcessingSpeech with its silence detection.
        // Otherwise silence detection fires after ~1.5s even if the user hasn't spoken yet.
        m_state = WaitingForSpeechStart;
        m_speechBuffer.clear();
        m_speechBuffer.reserve(16000 * 10);
        m_speechStartTimer->start();
        qDebug() << "PicovoiceManager: Waiting for user to start speaking (no Rhino)...";
        setStatusMessage("Listening...");
    }
}

void PicovoiceManager::processRhinoIntent(const int16_t *frame)
{
    if (!m_rhino) {
        // No Rhino, fall back to Leopard immediately
        m_state = ProcessingSpeech;
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_speechStartTime = now;
        m_lastVoiceActivityTime = now;
        m_speechBuffer.clear();
        m_speechBuffer.reserve(16000 * 10);
        return;
    }

    bool isFinalized = false;
    pv_status_t status = pv_rhino_process(m_rhino, frame, &isFinalized);

    if (status != PV_STATUS_SUCCESS) {
        qWarning() << "PicovoiceManager: Rhino process error:" << pv_status_to_string(status);
        return;
    }

    // Also accumulate audio for potential Leopard fallback
    for (int32_t i = 0; i < m_frameLength; ++i) {
        m_speechBuffer.append(frame[i]);
    }

    if (isFinalized) {
        m_commandTimer->stop();
        bool isUnderstood = false;
        pv_rhino_is_understood(m_rhino, &isUnderstood);

        if (isUnderstood) {
            // Rhino understood the intent
            const char *intent = nullptr;
            int32_t numSlots = 0;
            const char **slotNames = nullptr;
            const char **values = nullptr;

            status = pv_rhino_get_intent(m_rhino, &intent, &numSlots, &slotNames, &values);

            if (status == PV_STATUS_SUCCESS && intent) {
                qDebug() << "PicovoiceManager: Intent detected:" << intent;

                // Convert slots to QVariantMap
                QVariantMap slotMap;
                for (int32_t i = 0; i < numSlots; i++) {
                    slotMap[QString(slotNames[i])] = QString(values[i]);
                    qDebug() << "  Slot:" << slotNames[i] << "=" << values[i];
                }

                emit intentDetected(QString(intent), slotMap);

                // Free Rhino resources
                pv_rhino_free_slots_and_values(m_rhino, slotNames, values);
            }

            // Reset and return to listening
            pv_rhino_reset(m_rhino);
            resetToListening();
        } else {
            // Rhino didn't understand, use Leopard for full transcription
            qDebug() << "PicovoiceManager: Rhino didn't understand, using Leopard...";
            m_commandTimer->stop();
            m_state = ProcessingSpeech;
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            m_speechStartTime = now;
            m_lastVoiceActivityTime = now;  // Initialize VAD timestamp
            setStatusMessage("Processing complex query...");

            // Continue accumulating audio for a bit longer, then finalize
            // We'll finalize on timeout or silence detection
        }
    }
}

void PicovoiceManager::finalizeLeopardTranscription()
{
    if (m_speechBuffer.isEmpty()) {
        resetToListening();
        return;
    }

    // Prevent re-entry while STT is already processing
    if (m_googleSTT && m_googleSTT->isProcessing()) {
        return;
    }

    // Try Google STT first (better accuracy for names)
    if (m_googleSTT && !m_googleApiKey.isEmpty()) {
        qDebug() << "PicovoiceManager: Transcribing with Google STT..." << m_speechBuffer.size() << "samples";
        setStatusMessage("Sending to Google STT...");

        // Transition to WaitingForTranscription — stops silence detection from re-firing
        m_state = WaitingForTranscription;
        m_transcriptionTimer->start();

        // Send audio to Google STT - callback will handle result
        m_googleSTT->transcribe(m_speechBuffer);
        return;
    }

    // Fallback to Leopard if Google STT not available
    if (!m_leopard) {
        qWarning() << "PicovoiceManager: No STT engine available";
        resetToListening();
        return;
    }

    qDebug() << "PicovoiceManager: Transcribing with Leopard (fallback)..." << m_speechBuffer.size() << "samples";

    QString transcription = transcribeWithLeopard(m_speechBuffer);
    if (!transcription.isEmpty()) {
        qDebug() << "PicovoiceManager: Leopard transcription:" << transcription;
        if (!transcription.trimmed().isEmpty()) {
            emit transcriptionReady(transcription);
        } else {
            qDebug() << "PicovoiceManager: Empty transcription, ignoring";
        }
    }

    // Reset state
    resetToListening();
}

QString PicovoiceManager::transcribeWithLeopard(const QVector<int16_t> &audioBuffer)
{
    if (!m_leopard || audioBuffer.isEmpty()) {
        return QString();
    }

    char *transcript = nullptr;
    int32_t numWords = 0;
    pv_word_t *words = nullptr;

    pv_status_t status = pv_leopard_process(
        m_leopard,
        audioBuffer.data(),
        audioBuffer.size(),
        &transcript,
        &numWords,
        &words
    );

    QString result;
    if (status == PV_STATUS_SUCCESS && transcript) {
        result = QString::fromUtf8(transcript);
        pv_leopard_transcript_delete(transcript);
        pv_leopard_words_delete(words);
    } else {
        qWarning() << "PicovoiceManager: Leopard transcription error:" << pv_status_to_string(status);
    }

    return result;
}

void PicovoiceManager::onGoogleTranscriptionReady(const QString &text, float confidence)
{
    m_transcriptionTimer->stop();

    // Guard: only accept transcription results when we're actually waiting for one
    if (m_state != WaitingForTranscription) {
        qDebug() << "PicovoiceManager: Ignoring stale Google STT result (state:" << m_state << ")";
        return;
    }

    qDebug() << "PicovoiceManager: Google STT transcription:" << text << "confidence:" << confidence;

    if (text.trimmed().isEmpty()) {
        qDebug() << "PicovoiceManager: Empty Google STT result, returning to listening";
        m_speechBuffer.clear();
        m_speechBuffer.reserve(16000 * 10);
        resetToListening();
        return;
    }

    emit transcriptionReady(text);

    // Reset state and speech buffer
    m_speechBuffer.clear();
    m_speechBuffer.reserve(16000 * 10);
    resetToListening();
}

void PicovoiceManager::onGoogleError(const QString &message)
{
    m_transcriptionTimer->stop();

    // Ignore stale errors that arrive after we've moved to a different state
    if (m_state != WaitingForTranscription) {
        qDebug() << "PicovoiceManager: Ignoring stale Google STT error (state:" << m_state << "):" << message;
        return;
    }

    qWarning() << "PicovoiceManager: Google STT error:" << message;

    // Fall back to Leopard on Google STT failure
    if (m_leopard && !m_speechBuffer.isEmpty()) {
        qDebug() << "PicovoiceManager: Falling back to Leopard STT...";

        QString transcription = transcribeWithLeopard(m_speechBuffer);
        if (!transcription.isEmpty()) {
            qDebug() << "PicovoiceManager: Leopard fallback transcription:" << transcription;
            if (!transcription.trimmed().isEmpty()) {
                emit transcriptionReady(transcription);
            }
        }
    }

    // Reset state
    m_speechBuffer.clear();
    m_speechBuffer.reserve(16000 * 10);
    resetToListening();
}

void PicovoiceManager::manualActivate()
{
    if (!m_isRunning) {
        qWarning() << "PicovoiceManager: Cannot manually activate - pipeline not running";
        return;
    }

    qDebug() << "PicovoiceManager: Manual activation (button press)";

    // Clean up any in-flight state (e.g., pending STT, follow-up mode)
    // so stale results don't race with this new activation
    if (m_googleSTT && m_googleSTT->isProcessing()) {
        m_googleSTT->cancel();
    }
    m_followUpTimer->stop();
    m_speechStartTimer->stop();
    m_transcriptionTimer->stop();
    m_commandTimer->stop();
    m_isPaused = false;

    // Simulate wake word detection - go to WaitingForReadyPrompt
    m_lastDetectionTime = QDateTime::currentMSecsSinceEpoch();
    m_speechBuffer.clear();
    m_speechBuffer.reserve(16000 * 10);
    if (m_audioDevice) m_audioDevice->readAll();
    m_audioBuffer.clear();
    emit wakeWordDetected(m_wakeWord);

    m_state = WaitingForReadyPrompt;
    m_readyPromptTimer->start();
    setStatusMessage("Playing ready prompt...");
}

void PicovoiceManager::enterFollowUpMode()
{
    if (!m_isRunning) {
        qWarning() << "PicovoiceManager: Cannot enter follow-up mode - pipeline not running";
        return;
    }

    qDebug() << "PicovoiceManager: Entering follow-up mode (12s timeout)";
    m_state = WaitingForFollowUp;
    m_speechBuffer.clear();
    m_speechBuffer.reserve(16000 * 10);
    // Only start the timeout if not paused (TTS might still be playing).
    // If paused, the timer will start when resume() is called.
    if (!m_isPaused) {
        m_followUpTimer->start();
    }
    setStatusMessage("Listening for follow-up...");
}

void PicovoiceManager::cancelAndReset()
{
    qDebug() << "PicovoiceManager: Cancel and reset — returning to wake word listening";
    m_isPaused = false;
    // Cancel any in-flight Google STT request so stale results don't arrive later
    if (m_googleSTT && m_googleSTT->isProcessing()) {
        m_googleSTT->cancel();
    }
    m_transcriptionTimer->stop();
    resetToListening();
}

void PicovoiceManager::resetToListening()
{
    // Discard any stale audio that accumulated during STT processing
    if (m_audioDevice) m_audioDevice->readAll();
    m_audioBuffer.clear();
    m_speechBuffer.clear();
    m_speechBuffer.reserve(16000 * 10);
    m_readyPromptTimer->stop();
    m_followUpTimer->stop();
    m_speechStartTimer->stop();
    m_transcriptionTimer->stop();
    m_commandTimer->stop();
    m_state = Listening;

    // Reset Rhino if it was used
    if (m_rhino) {
        pv_rhino_reset(m_rhino);
    }

    if (m_wakeWordAvailable) {
        setStatusMessage(QString("Listening for '%1'...").arg(m_wakeWord));
    } else {
        setStatusMessage("Ready (button activation only)");
    }
}

// ========== INITIALIZATION ==========

bool PicovoiceManager::initializeKoala()
{
    if (m_koala) {
        qWarning() << "PicovoiceManager: Koala already initialized";
        return true;
    }

    qDebug() << "PicovoiceManager: Initializing Koala noise suppression...";

    // Koala v3 requires model path and device parameter
    QString koalaModelPath = getModelPath("koala");
    QByteArray accessKeyBytes = m_accessKey.toUtf8();
    QByteArray modelPathBytes = koalaModelPath.toUtf8();
    pv_status_t status = pv_koala_init(
        m_accessKey.isEmpty() ? nullptr : accessKeyBytes.constData(),
        modelPathBytes.constData(),
        "best",   // Auto-select best device (CPU/GPU)
        &m_koala
    );

    if (status != PV_STATUS_SUCCESS) {
        QString statusStr = pv_status_to_string(status);
        qCritical() << "PicovoiceManager: Failed to initialize Koala:" << statusStr;
        if (statusStr == "INVALID_ARGUMENT") {
            qCritical() << "  Likely cause: Access key invalid/expired or Koala SDK library incompatible";
        }
        return false;
    }

    m_koalaFrameLength = pv_koala_frame_length();
    pv_koala_delay_sample(m_koala, &m_koalaDelaySamples);

    qDebug() << "PicovoiceManager: Koala initialized";
    qDebug() << "  Frame length:" << m_koalaFrameLength << "samples";
    qDebug() << "  Delay:" << m_koalaDelaySamples << "samples";
    qDebug() << "  Version:" << pv_koala_version();

    return true;
}

bool PicovoiceManager::initializePorcupine()
{
    if (m_porcupine) {
        qWarning() << "PicovoiceManager: Porcupine already initialized";
        return true;
    }

    qDebug() << "PicovoiceManager: Initializing Porcupine wake word detection...";

    QString modelPath = getModelPath("porcupine");
    QString keywordPath = getKeywordPath();

    qDebug() << "  Model path:" << modelPath;
    qDebug() << "  Keyword path:" << keywordPath;

    if (!QFile::exists(modelPath)) {
        qCritical() << "PicovoiceManager: Model file not found:" << modelPath;
        return false;
    }

    if (!QFile::exists(keywordPath)) {
        qCritical() << "PicovoiceManager: Keyword file not found:" << keywordPath;
        return false;
    }

    QByteArray accessKeyBytes = m_accessKey.toUtf8();
    QByteArray modelPathBytes = modelPath.toUtf8();
    QByteArray keywordPathBytes = keywordPath.toUtf8();
    const char *accessKey = m_accessKey.isEmpty() ? nullptr : accessKeyBytes.constData();
    const char *modelPathC = modelPathBytes.constData();
    const char *keywordPathC = keywordPathBytes.constData();
    const char *keywordPaths[] = { keywordPathC };
    const float sensitivities[] = { m_sensitivity };

    pv_status_t status = pv_porcupine_init(
        accessKey,
        modelPathC,
        1,  // num_keywords
        keywordPaths,
        sensitivities,
        &m_porcupine
    );

    if (status != PV_STATUS_SUCCESS) {
        QString statusStr = pv_status_to_string(status);
        qCritical() << "PicovoiceManager: Failed to initialize Porcupine:" << statusStr;

        if (statusStr == "INVALID_ARGUMENT") {
            qCritical() << "  Likely cause: Picovoice access key is invalid, expired, or incompatible with SDK v" << pv_porcupine_version();
            qCritical() << "  Get a new key at: https://console.picovoice.ai/";
        } else if (statusStr == "KEY_ERROR") {
            qCritical() << "  Access key validation failed. Check your PICOVOICE_ACCESS_KEY in .env";
        } else if (statusStr == "IO_ERROR") {
            qCritical() << "  File read error - model or keyword file may be corrupted";
        }

        return false;
    }

    m_frameLength = pv_porcupine_frame_length();
    m_sampleRate = pv_sample_rate();

    qDebug() << "PicovoiceManager: Porcupine initialized";
    qDebug() << "  Frame length:" << m_frameLength << "samples";
    qDebug() << "  Sample rate:" << m_sampleRate << "Hz";
    qDebug() << "  Version:" << pv_porcupine_version();

    return true;
}

bool PicovoiceManager::initializeRhino()
{
    if (m_rhino) {
        qWarning() << "PicovoiceManager: Rhino already initialized";
        return true;
    }

    qDebug() << "PicovoiceManager: Initializing Rhino speech-to-intent...";

    QString modelPath = getModelPath("rhino");
    QString contextPath = m_rhinoContextPath;

    // If no custom context path, try default location
    if (contextPath.isEmpty()) {
        contextPath = basePath() + "/external/rhino/contexts/headunit_raspberry-pi.rhn";
    }

    qDebug() << "  Model path:" << modelPath;
    qDebug() << "  Context path:" << contextPath;

    if (!QFile::exists(modelPath)) {
        qCritical() << "PicovoiceManager: Rhino model file not found:" << modelPath;
        return false;
    }

    if (!QFile::exists(contextPath)) {
        qCritical() << "PicovoiceManager: Rhino context file not found:" << contextPath;
        qCritical() << "  Please create the context file using Picovoice Console (https://console.picovoice.ai/)";
        return false;
    }

    QByteArray accessKeyBytes = m_accessKey.toUtf8();
    QByteArray modelPathBytes = modelPath.toUtf8();
    QByteArray contextPathBytes = contextPath.toUtf8();
    const char *accessKey = m_accessKey.isEmpty() ? nullptr : accessKeyBytes.constData();
    const char *modelPathC = modelPathBytes.constData();
    const char *contextPathC = contextPathBytes.constData();

    pv_status_t status = pv_rhino_init(
        accessKey,
        modelPathC,
        contextPathC,
        m_sensitivity,      // sensitivity
        1.0f,               // endpoint_duration_sec
        false,              // require_endpoint
        &m_rhino            // output handle
    );

    if (status != PV_STATUS_SUCCESS) {
        qCritical() << "PicovoiceManager: Failed to initialize Rhino:" << pv_status_to_string(status);
        return false;
    }

    qDebug() << "PicovoiceManager: Rhino initialized";
    qDebug() << "  Frame length:" << pv_rhino_frame_length() << "samples";
    qDebug() << "  Version:" << pv_rhino_version();

    // Get context info
    const char *contextInfo = nullptr;
    pv_rhino_context_info(m_rhino, &contextInfo);
    if (contextInfo) {
        qDebug() << "  Context info:" << contextInfo;
    }

    return true;
}

bool PicovoiceManager::initializeLeopard()
{
    if (m_leopard) {
        qWarning() << "PicovoiceManager: Leopard already initialized";
        return true;
    }

    qDebug() << "PicovoiceManager: Initializing Leopard speech-to-text...";

    QString modelPath = getModelPath("leopard");

    qDebug() << "  Model path:" << modelPath;

    if (!QFile::exists(modelPath)) {
        qCritical() << "PicovoiceManager: Leopard model file not found:" << modelPath;
        return false;
    }

    QByteArray accessKeyBytes = m_accessKey.toUtf8();
    QByteArray modelPathBytes = modelPath.toUtf8();
    const char *accessKey = m_accessKey.isEmpty() ? nullptr : accessKeyBytes.constData();
    const char *modelPathC = modelPathBytes.constData();

    pv_status_t status = pv_leopard_init(
        accessKey,
        modelPathC,
        "best",     // device (auto-select CPU/GPU)
        true,       // enable_automatic_punctuation
        false,      // enable_diarization
        &m_leopard  // output handle
    );

    if (status != PV_STATUS_SUCCESS) {
        qCritical() << "PicovoiceManager: Failed to initialize Leopard:" << pv_status_to_string(status);
        return false;
    }

    qDebug() << "PicovoiceManager: Leopard initialized";
    qDebug() << "  Version:" << pv_leopard_version();

    return true;
}

// ========== CLEANUP ==========

void PicovoiceManager::cleanup()
{
    cleanupKoala();
    cleanupPorcupine();
    cleanupRhino();
    cleanupLeopard();
    m_isInitialized = false;
    emit initializedChanged();
}

void PicovoiceManager::cleanupKoala()
{
    if (m_koala) {
        pv_koala_delete(m_koala);
        m_koala = nullptr;
    }
}

void PicovoiceManager::cleanupPorcupine()
{
    if (m_porcupine) {
        pv_porcupine_delete(m_porcupine);
        m_porcupine = nullptr;
    }
}

void PicovoiceManager::cleanupRhino()
{
    if (m_rhino) {
        pv_rhino_delete(m_rhino);
        m_rhino = nullptr;
    }
}

void PicovoiceManager::cleanupLeopard()
{
    if (m_leopard) {
        pv_leopard_delete(m_leopard);
        m_leopard = nullptr;
    }
}

// ========== PATH HELPERS ==========

QString PicovoiceManager::basePath()
{
    // Use application directory to find external SDKs (portable)
    static QString base = QCoreApplication::applicationDirPath() + "/..";
    return base;
}

QString PicovoiceManager::getModelPath(const QString &component) const
{
    QString base = basePath() + "/external";
    if (component == "porcupine") {
        return base + "/porcupine/lib/common/porcupine_params.pv";
    } else if (component == "rhino") {
        return base + "/rhino/common/rhino_params.pv";
    } else if (component == "leopard") {
        return base + "/leopard/common/leopard_params.pv";
    } else if (component == "koala") {
        return base + "/koala/common/koala_params.pv";
    }
    return QString();
}

QString PicovoiceManager::getKeywordPath() const
{
    QString base = basePath() + "/external/porcupine/resources/keyword_files/raspberry-pi";

    // Check for custom "Hey Sammy" wake word (must be raspberry-pi platform to match library)
    if (m_wakeWord.toLower() == "hey sammy") {
        QString customPath = base + "/Hey-Sammy_en_raspberry-pi.ppn";
        if (QFile::exists(customPath)) {
            return customPath;
        }
        // Fallback to jarvis if Hey Sammy file not found
        qWarning() << "PicovoiceManager: Hey Sammy wake word file not found. Please download the Raspberry Pi version from Picovoice Console.";
        qWarning() << "PicovoiceManager: Falling back to 'jarvis' wake word";
        return base + "/jarvis_raspberry-pi.ppn";
    }

    // For built-in wake words, use the standard path
    QString keywordName = m_wakeWord;
    keywordName.replace("_", " ");
    return QString("%1/%2_raspberry-pi.ppn").arg(base, keywordName);
}

void PicovoiceManager::setStatusMessage(const QString &msg)
{
    if (m_statusMessage == msg) {
        return;
    }

    m_statusMessage = msg;
    emit statusMessageChanged();
}

int16_t PicovoiceManager::calculateFrameEnergy(const int16_t *frame, int32_t length) const
{
    // Calculate RMS (Root Mean Square) energy of the audio frame
    if (!frame || length <= 0) {
        return 0;
    }

    int64_t sumSquares = 0;
    for (int32_t i = 0; i < length; ++i) {
        int64_t sample = static_cast<int64_t>(frame[i]);
        sumSquares += sample * sample;
    }

    // Return RMS as int16_t (approximation using integer math)
    return static_cast<int16_t>(qSqrt(static_cast<double>(sumSquares) / length));
}
