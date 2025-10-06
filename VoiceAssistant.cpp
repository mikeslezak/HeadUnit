#include "VoiceAssistant.h"
#include <QDebug>
#include <QSettings>

/**
 * CONSTRUCTOR
 *
 * Initializes VoiceAssistant with TTS engine (if available) and default settings
 */
VoiceAssistant::VoiceAssistant(QObject *parent)
    : QObject(parent)
    , m_isConnected(false)
    , m_isListening(false)
    , m_isVoiceActive(false)
    , m_autoReadMessages(true)
    , m_voiceVolume(80)
    , m_speechRate(0.0)
    , m_activeAssistant("none")
    , m_statusMessage("Not connected")
    , m_tts(nullptr)
#ifndef Q_OS_WIN
    , m_socket(nullptr)
#endif
{
#ifdef Q_OS_WIN
    qDebug() << "VoiceAssistant: Mock mode (Windows)";
    m_statusMessage = "Mock Mode - Voice features simulated";
#else
    qDebug() << "VoiceAssistant: Real Bluetooth mode";
    m_statusMessage = "Ready to connect";
#endif

#if HAS_TEXT_TO_SPEECH
    qDebug() << "TextToSpeech: Available";
#else
    qDebug() << "TextToSpeech: Not available - using phone's voice only";
#endif

    // Load saved settings
    QSettings settings;
    m_autoReadMessages = settings.value("voice/autoReadMessages", true).toBool();
    m_voiceVolume = settings.value("voice/volume", 80).toInt();
    m_speechRate = settings.value("voice/speechRate", 0.0).toDouble();

    // Initialize quick reply templates
    m_quickReplies = {
        "I'm driving, I'll call you back",
        "On my way",
        "Running late, be there soon",
        "Yes",
        "No",
        "Thanks!",
        "Can't talk now",
        "Send me a text"
    };

    // Setup text-to-speech if available
#if HAS_TEXT_TO_SPEECH
    m_tts = new QTextToSpeech(this);

    if (m_tts->state() == QTextToSpeech::Error) {
        qWarning() << "TTS initialization failed";
        delete m_tts;
        m_tts = nullptr;
    } else {
        m_tts->setRate(m_speechRate);
        m_tts->setVolume(m_voiceVolume / 100.0);

        connect(m_tts, &QTextToSpeech::stateChanged, this, [this](QTextToSpeech::State state) {
            if (state == QTextToSpeech::Speaking) {
                m_isVoiceActive = true;
                emit voiceActiveChanged();
            } else if (state == QTextToSpeech::Ready) {
                m_isVoiceActive = false;
                emit voiceActiveChanged();
            }
        });

        qDebug() << "Available TTS voices:";
        for (const QVoice &voice : m_tts->availableVoices()) {
            qDebug() << "  -" << voice.name() << voice.gender() << voice.age();
        }
    }
#endif
}

/**
 * DESTRUCTOR
 */
VoiceAssistant::~VoiceAssistant()
{
#if HAS_TEXT_TO_SPEECH
    if (m_tts) {
        delete m_tts;
    }
#endif

#ifndef Q_OS_WIN
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
    }
#endif
}

// ========================================================================
// CONNECTION MANAGEMENT
// ========================================================================

/**
 * CONNECT TO PHONE
 *
 * Establishes HFP (Hands-Free Profile) connection for voice features
 */
void VoiceAssistant::connectToPhone(const QString &deviceAddress)
{
    m_deviceAddress = deviceAddress;

#ifdef Q_OS_WIN
    // Mock connection
    QTimer::singleShot(1000, this, [this]() {
        m_isConnected = true;
        m_activeAssistant = "Siri";
        emit connectionChanged();
        emit activeAssistantChanged();
        setStatusMessage("Mock: Connected - Voice features available");
    });
#else
    // Real Bluetooth HFP connection
    setStatusMessage("Connecting to voice services...");

    // HFP service UUID: 0x111E (Hands-Free)
    static const QBluetoothUuid hfpUuid(QBluetoothUuid::ServiceClassUuid::Handsfree);

    m_bluetoothAddress = QBluetoothAddress(deviceAddress);

    if (m_socket) {
        m_socket->deleteLater();
    }

    m_socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);

    connect(m_socket, &QBluetoothSocket::connected,
            this, &VoiceAssistant::onSocketConnected);
    connect(m_socket, &QBluetoothSocket::disconnected,
            this, &VoiceAssistant::onSocketDisconnected);
    connect(m_socket, &QBluetoothSocket::errorOccurred,
            this, &VoiceAssistant::onSocketError);
    connect(m_socket, &QBluetoothSocket::readyRead,
            this, &VoiceAssistant::onSocketReadyRead);

    m_socket->connectToService(m_bluetoothAddress, hfpUuid);
#endif
}

/**
 * DISCONNECT
 */
void VoiceAssistant::disconnect()
{
#ifdef Q_OS_WIN
    m_isConnected = false;
    m_activeAssistant = "none";
    emit connectionChanged();
    emit activeAssistantChanged();
    setStatusMessage("Mock: Disconnected");
#else
    if (m_socket) {
        m_socket->close();
    }
    m_isConnected = false;
    m_activeAssistant = "none";
    emit connectionChanged();
    emit activeAssistantChanged();
    setStatusMessage("Disconnected");
#endif
}

// ========================================================================
// VOICE ASSISTANT ACTIVATION
// ========================================================================

/**
 * ACTIVATE VOICE ASSISTANT
 *
 * Triggers Siri (iOS) or Google Assistant (Android)
 * Uses HFP AT command: AT+BVRA=1
 */
void VoiceAssistant::activateAssistant()
{
    if (!m_isConnected) {
        emit error("Not connected to phone");
        return;
    }

#ifdef Q_OS_WIN
    // Mock activation
    m_isListening = true;
    emit listeningChanged();
    setStatusMessage("Mock: Listening... (say 'Call Mom')");

    // Simulate voice command recognition
    QTimer::singleShot(3000, this, [this]() {
        m_isListening = false;
        emit listeningChanged();
        setStatusMessage("Mock: Command recognized");
        emit commandRecognized("Call Mom");
    });
#else
    // Send Bluetooth Voice Recognition Activation command
    sendHFPCommand("AT+BVRA=1");
    m_isListening = true;
    emit listeningChanged();
    setStatusMessage("Voice assistant activated");
#endif
}

/**
 * DEACTIVATE VOICE ASSISTANT
 */
void VoiceAssistant::deactivateAssistant()
{
#ifdef Q_OS_WIN
    m_isListening = false;
    emit listeningChanged();
    setStatusMessage("Mock: Voice deactivated");
#else
    sendHFPCommand("AT+BVRA=0");
    m_isListening = false;
    emit listeningChanged();
    setStatusMessage("Voice deactivated");
#endif
}

// ========================================================================
// TEXT-TO-SPEECH
// ========================================================================

/**
 * SPEAK TEXT
 *
 * Converts text to speech and plays it through speakers
 */
void VoiceAssistant::speak(const QString &text)
{
#if HAS_TEXT_TO_SPEECH
    if (!m_tts) {
        qWarning() << "TTS not available";
        emit error("Text-to-speech not available");
        return;
    }

    if (m_tts->state() == QTextToSpeech::Speaking) {
        m_tts->stop();
    }

    qDebug() << "Speaking:" << text;
    m_tts->say(text);
    setStatusMessage("Speaking...");
#else
    qDebug() << "Would speak:" << text << "(TTS not available)";
    setStatusMessage("TTS not available - use phone's voice");
    emit error("Local text-to-speech not available");
#endif
}

/**
 * STOP SPEAKING
 */
void VoiceAssistant::stopSpeaking()
{
#if HAS_TEXT_TO_SPEECH
    if (m_tts && m_tts->state() == QTextToSpeech::Speaking) {
        m_tts->stop();
        setStatusMessage("Stopped");
    }
#else
    qDebug() << "Stop speaking (TTS not available)";
#endif
}

/**
 * READ MESSAGE ALOUD
 *
 * Speaks incoming message with sender name
 */
void VoiceAssistant::readMessage(const QString &sender, const QString &message)
{
    if (!m_autoReadMessages) {
        qDebug() << "Auto-read disabled, not speaking message";
        return;
    }

#if HAS_TEXT_TO_SPEECH
    QString announcement = QString("Message from %1: %2").arg(sender, message);
    speak(announcement);
    emit messageRead(sender, message);
#else
    qDebug() << "Would read message from" << sender << ":" << message;
    qDebug() << "(TTS not available - phone's assistant can read it)";
    emit messageRead(sender, message);
#endif
}

/**
 * ANNOUNCE CALLER
 *
 * Speaks caller name for incoming calls
 */
void VoiceAssistant::announceCaller(const QString &callerName)
{
#if HAS_TEXT_TO_SPEECH
    QString announcement = QString("Incoming call from %1").arg(callerName);
    speak(announcement);
#else
    qDebug() << "Would announce caller:" << callerName;
    qDebug() << "(TTS not available - phone will announce)";
#endif
}

// ========================================================================
// VOICE SETTINGS
// ========================================================================

/**
 * SET AUTO-READ MESSAGES
 */
void VoiceAssistant::setAutoReadMessages(bool enabled)
{
    if (m_autoReadMessages != enabled) {
        m_autoReadMessages = enabled;
        emit autoReadMessagesChanged();

        QSettings settings;
        settings.setValue("voice/autoReadMessages", enabled);

        setStatusMessage(enabled ? "Auto-read enabled" : "Auto-read disabled");
    }
}

/**
 * SET VOICE VOLUME
 */
void VoiceAssistant::setVoiceVolume(int volume)
{
    volume = qBound(0, volume, 100);

    if (m_voiceVolume != volume) {
        m_voiceVolume = volume;
        emit voiceVolumeChanged();

#if HAS_TEXT_TO_SPEECH
        if (m_tts) {
            m_tts->setVolume(volume / 100.0);
        }
#endif

        QSettings settings;
        settings.setValue("voice/volume", volume);
    }
}

/**
 * SET SPEECH RATE
 *
 * @param rate: -1.0 (slow) to 1.0 (fast), 0.0 = normal
 */
void VoiceAssistant::setSpeechRate(double rate)
{
    rate = qBound(-1.0, rate, 1.0);

    if (m_speechRate != rate) {
        m_speechRate = rate;
        emit speechRateChanged();

#if HAS_TEXT_TO_SPEECH
        if (m_tts) {
            m_tts->setRate(rate);
        }
#endif

        QSettings settings;
        settings.setValue("voice/speechRate", rate);
    }
}

// ========================================================================
// QUICK REPLIES
// ========================================================================

/**
 * SEND QUICK REPLY
 *
 * Sends pre-defined text response to last message sender
 */
void VoiceAssistant::sendQuickReply(int index)
{
    if (index < 0 || index >= m_quickReplies.size()) {
        emit error("Invalid quick reply index");
        return;
    }

    QString reply = m_quickReplies[index];
    qDebug() << "Quick reply:" << reply;

    // In real implementation, this would send via Bluetooth MAP (Message Access Profile)
    // For now, just emit signal for UI feedback
    emit replySent(reply);
    setStatusMessage("Reply sent: " + reply);
}

/**
 * ADD CUSTOM QUICK REPLY
 */
void VoiceAssistant::addQuickReply(const QString &text)
{
    if (!text.isEmpty() && !m_quickReplies.contains(text)) {
        m_quickReplies.append(text);
        emit quickRepliesChanged();

        QSettings settings;
        settings.setValue("voice/quickReplies", m_quickReplies);
    }
}

/**
 * REMOVE QUICK REPLY
 */
void VoiceAssistant::removeQuickReply(int index)
{
    if (index >= 0 && index < m_quickReplies.size()) {
        m_quickReplies.removeAt(index);
        emit quickRepliesChanged();

        QSettings settings;
        settings.setValue("voice/quickReplies", m_quickReplies);
    }
}

// ========================================================================
// VOICE COMMANDS
// ========================================================================

/**
 * PROCESS VOICE COMMAND
 *
 * Interprets recognized text and executes corresponding action
 */
void VoiceAssistant::processVoiceCommand(const QString &command)
{
    QString cmd = command.toLower().trimmed();
    qDebug() << "Processing command:" << cmd;

    // Call commands
    if (cmd.startsWith("call ")) {
        QString contact = cmd.mid(5);
        emit commandRecognized("call:" + contact);
    }
    // Navigation commands
    else if (cmd.startsWith("navigate to ") || cmd.startsWith("go to ")) {
        QString destination = cmd.contains("navigate to ") ? cmd.mid(12) : cmd.mid(6);
        emit commandRecognized("navigate:" + destination);
    }
    // Music commands
    else if (cmd.startsWith("play ")) {
        QString query = cmd.mid(5);
        emit commandRecognized("play:" + query);
    }
    else if (cmd == "pause" || cmd == "stop") {
        emit commandRecognized("pause");
    }
    else if (cmd == "next" || cmd == "next song" || cmd == "skip") {
        emit commandRecognized("next");
    }
    else if (cmd == "previous" || cmd == "previous song" || cmd == "back") {
        emit commandRecognized("previous");
    }
    // Message commands
    else if (cmd.startsWith("send message ") || cmd.startsWith("text ")) {
        emit commandRecognized("message:" + cmd);
    }
    else if (cmd == "read messages") {
        emit commandRecognized("readMessages");
    }
    // General
    else {
        emit commandRecognized(cmd);
    }
}

// ========================================================================
// BLUETOOTH COMMUNICATION (Real Mode Only)
// ========================================================================

#ifndef Q_OS_WIN

void VoiceAssistant::onSocketConnected()
{
    m_isConnected = true;
    emit connectionChanged();
    setStatusMessage("Voice services connected");

    // Detect which assistant is available
    sendHFPCommand("AT+BRSF=?");  // Query supported features
}

void VoiceAssistant::onSocketDisconnected()
{
    m_isConnected = false;
    m_activeAssistant = "none";
    emit connectionChanged();
    emit activeAssistantChanged();
    setStatusMessage("Voice services disconnected");
}

void VoiceAssistant::onSocketError()
{
    qWarning() << "Voice socket error:" << m_socket->errorString();
    emit error("Connection error: " + m_socket->errorString());
}

void VoiceAssistant::onSocketReadyRead()
{
    QByteArray data = m_socket->readAll();
    QString response = QString::fromUtf8(data);
    parseHFPResponse(response);
}

#endif

/**
 * SEND HFP COMMAND
 *
 * Sends AT command to phone via Hands-Free Profile
 */
void VoiceAssistant::sendHFPCommand(const QString &command)
{
#ifdef Q_OS_WIN
    qDebug() << "Mock HFP command:" << command;
#else
    if (!m_socket || !m_socket->isOpen()) {
        qWarning() << "Cannot send command - socket not connected";
        return;
    }

    QByteArray packet = (command + "\r\n").toUtf8();
    m_socket->write(packet);
    qDebug() << "Sent HFP command:" << command;
#endif
}

/**
 * PARSE HFP RESPONSE
 *
 * Handles responses from phone
 */
void VoiceAssistant::parseHFPResponse(const QString &response)
{
    qDebug() << "HFP response:" << response;

    if (response.contains("+BRSF")) {
        // Phone features response
        if (response.contains("Siri")) {
            m_activeAssistant = "Siri";
        } else if (response.contains("Google")) {
            m_activeAssistant = "Google Assistant";
        } else {
            m_activeAssistant = "Generic";
        }
        emit activeAssistantChanged();
    }
    else if (response.contains("+BVRA")) {
        // Voice recognition state
        if (response.contains("+BVRA:1")) {
            m_isListening = true;
        } else {
            m_isListening = false;
        }
        emit listeningChanged();
    }
}

/**
 * SET STATUS MESSAGE
 */
void VoiceAssistant::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "VoiceAssistant:" << msg;
    }
}
