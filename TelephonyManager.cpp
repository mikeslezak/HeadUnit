#include "TelephonyManager.h"
#include "BluetoothDeviceModel.h"
#include "ContactManager.h"
#include <QDebug>
#include <QRegularExpression>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

TelephonyManager::TelephonyManager(QObject *parent)
    : QObject(parent)
    , m_hasActiveCall(false)
    , m_activeCallDuration(0)
    , m_isCallMuted(false)
    , m_cellularSignal(0)
    , m_ofonoUpdateTimer(new QTimer(this))
    , m_callDurationTimer(new QTimer(this))
    , m_contactManager(nullptr)
    , m_deviceModel(nullptr)
    , m_adapterPath("/org/bluez/hci0")
{
#ifdef Q_OS_WIN
    m_mockMode = true;
#else
    m_mockMode = false;
#endif

    // oFono signal polling (every 10 seconds)
    connect(m_ofonoUpdateTimer, &QTimer::timeout, this, &TelephonyManager::updateOfonoSignal);
    m_ofonoUpdateTimer->setInterval(10000);
    m_ofonoUpdateTimer->start();

    // Call duration counter (every second while active)
    connect(m_callDurationTimer, &QTimer::timeout, this, &TelephonyManager::updateCallDuration);
    m_callDurationTimer->setInterval(1000);
}

void TelephonyManager::setContactManager(ContactManager* contactManager)
{
    m_contactManager = contactManager;
}

void TelephonyManager::setDeviceModel(BluetoothDeviceModel* model)
{
    m_deviceModel = model;
}

void TelephonyManager::setAdapterPath(const QString& path)
{
    m_adapterPath = path;
}

// ========================================================================
// OFONO DBUS MONITORING SETUP
// ========================================================================

void TelephonyManager::setupOfonoMonitoring()
{
#ifndef Q_OS_WIN
    QDBusInterface ofonoManager(
        "org.ofono",
        "/",
        "org.ofono.Manager",
        QDBusConnection::systemBus()
    );

    if (ofonoManager.isValid()) {
        QDBusReply<QDBusArgument> reply = ofonoManager.call("GetModems");
        if (reply.isValid()) {
            QDBusArgument arg = reply.value();
            arg.beginArray();
            if (!arg.atEnd()) {
                QString modemPath;
                arg.beginStructure();
                arg >> modemPath;
                arg.endStructure();
                arg.endArray();

                qDebug() << "TelephonyManager: Monitoring oFono calls on modem:" << modemPath;

                // Monitor for incoming calls
                QDBusConnection::systemBus().connect(
                    "org.ofono",
                    modemPath,
                    "org.ofono.VoiceCallManager",
                    "CallAdded",
                    this,
                    SLOT(onCallAdded(QDBusObjectPath,QVariantMap))
                );

                // Monitor for call removal
                QDBusConnection::systemBus().connect(
                    "org.ofono",
                    modemPath,
                    "org.ofono.VoiceCallManager",
                    "CallRemoved",
                    this,
                    SLOT(onCallRemoved(QDBusObjectPath))
                );
            } else {
                arg.endArray();
            }
        }
    }

    qDebug() << "TelephonyManager: oFono monitoring setup complete";
#endif
}

// ========================================================================
// OFONO SIGNAL READING
// ========================================================================

void TelephonyManager::updateOfonoSignal()
{
#ifndef Q_OS_WIN
    if (!m_deviceModel) return;

    // Find connected device address from device model
    QString connectedAddress;
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::ConnectedRole).toBool()) {
            connectedAddress = m_deviceModel->data(index, BluetoothDeviceModel::AddressRole).toString();
            break;
        }
    }

    if (connectedAddress.isEmpty()) {
        if (m_cellularSignal != 0 || !m_carrierName.isEmpty()) {
            m_cellularSignal = 0;
            m_carrierName = "";
            emit cellularSignalChanged();
            emit carrierNameChanged();
        }
        return;
    }

    // Build oFono modem path from device address
    QString modemPath = "/hfp" + m_adapterPath + "/dev_" + connectedAddress.replace(":", "_");

    // Query oFono NetworkRegistration interface
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.ofono",
        modemPath,
        "org.ofono.NetworkRegistration",
        "GetProperties"
    );

    QDBusMessage reply = QDBusConnection::systemBus().call(msg);

    if (reply.type() != QDBusMessage::ReplyMessage) {
        return;
    }

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    QVariantMap properties;

    arg.beginMap();
    while (!arg.atEnd()) {
        QString key;
        QVariant value;

        arg.beginMapEntry();
        arg >> key;

        QDBusVariant dbusVariant;
        arg >> dbusVariant;
        value = dbusVariant.variant();

        arg.endMapEntry();

        properties[key] = value;
    }
    arg.endMap();

    // Convert signal strength (0-100) to bars (0-4)
    int strengthPercent = properties.value("Strength", 0).toInt();
    int signalBars = 0;
    if (strengthPercent >= 80) signalBars = 4;
    else if (strengthPercent >= 60) signalBars = 3;
    else if (strengthPercent >= 40) signalBars = 2;
    else if (strengthPercent >= 20) signalBars = 1;

    QString carrier = properties.value("Name", "").toString();

    bool changed = false;
    if (m_cellularSignal != signalBars) {
        m_cellularSignal = signalBars;
        emit cellularSignalChanged();
        changed = true;
    }
    if (m_carrierName != carrier) {
        m_carrierName = carrier;
        emit carrierNameChanged();
        changed = true;
    }

    if (changed) {
        qDebug() << "TelephonyManager: Cellular signal -"
                 << "Bars:" << m_cellularSignal
                 << "(" << strengthPercent << "%)"
                 << "Carrier:" << m_carrierName;
    }
#endif
}

// ========================================================================
// PHONE CALL METHODS
// ========================================================================

void TelephonyManager::dialNumber(const QString &phoneNumber)
{
#ifdef Q_OS_WIN
    qDebug() << "TelephonyManager: Mock dial" << phoneNumber;
#else
    if (phoneNumber.isEmpty()) {
        qWarning() << "TelephonyManager: Cannot dial empty number";
        return;
    }

    qDebug() << "TelephonyManager: Attempting to dial" << phoneNumber;

    QDBusInterface ofonoManager(
        "org.ofono",
        "/",
        "org.ofono.Manager",
        QDBusConnection::systemBus()
    );

    QString modemPath;
    if (!ofonoManager.isValid()) {
        qWarning() << "TelephonyManager: oFono Manager not available:" << ofonoManager.lastError().message();
        return;
    }

    QDBusMessage reply = ofonoManager.call("GetModems");
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "TelephonyManager: Failed to get oFono modems:" << reply.errorMessage();
        return;
    }

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginArray();

    while (!arg.atEnd()) {
        arg.beginStructure();
        arg >> modemPath;

        QVariantMap properties;
        arg >> properties;

        arg.endStructure();

        if (!modemPath.isEmpty()) {
            break;
        }
    }
    arg.endArray();

    if (modemPath.isEmpty()) {
        qWarning() << "TelephonyManager: No oFono modems found";
        return;
    }

    QDBusInterface vcm(
        "org.ofono",
        modemPath,
        "org.ofono.VoiceCallManager",
        QDBusConnection::systemBus()
    );

    if (!vcm.isValid()) {
        qWarning() << "TelephonyManager: VoiceCallManager not available:" << vcm.lastError().message();
        return;
    }

    // Clean phone number - only digits and +
    QString cleanNumber = phoneNumber;
    cleanNumber.remove(QRegularExpression("[^0-9+]"));

    qDebug() << "TelephonyManager: Cleaned number:" << cleanNumber << "(from" << phoneNumber << ")";

    // Set up call state optimistically
    m_activeCallNumber = phoneNumber;
    m_activeCallName = "";
    m_activeCallState = "dialing";
    m_activeCallDuration = 0;
    m_callStartTime = QDateTime::currentDateTime();
    m_hasActiveCall = true;
    m_callDurationTimer->start();
    emit activeCallChanged();

    QDBusMessage dialMsg = QDBusMessage::createMethodCall(
        "org.ofono", vcm.path(), "org.ofono.VoiceCallManager", "Dial");
    dialMsg << cleanNumber << QString("");

    auto pending = QDBusConnection::systemBus().asyncCall(dialMsg);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, phoneNumber](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            qWarning() << "TelephonyManager: Dial failed:" << reply.error().message();
            m_hasActiveCall = false;
            m_activeCallState.clear();
            m_callDurationTimer->stop();
            emit activeCallChanged();
        } else {
            QString callPath = reply.value().path();
            qDebug() << "TelephonyManager: Call initiated, path:" << callPath;
            m_activeCallPath = callPath;

            QDBusConnection::systemBus().connect(
                "org.ofono", callPath, "org.ofono.VoiceCall", "PropertyChanged",
                this, SLOT(onCallPropertyChanged(QString,QDBusVariant)));
        }
        w->deleteLater();
    });
#endif
}

void TelephonyManager::answerCall()
{
#ifdef Q_OS_WIN
    qDebug() << "TelephonyManager: Mock answer call";
#else
    if (m_activeCallPath.isEmpty()) {
        qWarning() << "TelephonyManager: No active call to answer";
        return;
    }

    qDebug() << "TelephonyManager: Answering call:" << m_activeCallPath;

    QDBusMessage answerMsg = QDBusMessage::createMethodCall(
        "org.ofono", m_activeCallPath, "org.ofono.VoiceCall", "Answer");

    auto pending = QDBusConnection::systemBus().asyncCall(answerMsg);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            qWarning() << "TelephonyManager: Answer failed:" << reply.error().message();
        } else {
            qDebug() << "TelephonyManager: Call answered successfully";
        }
        w->deleteLater();
    });
#endif
}

void TelephonyManager::hangupCall()
{
#ifdef Q_OS_WIN
    qDebug() << "TelephonyManager: Mock hangup";
#else
    qDebug() << "TelephonyManager: Attempting to hang up call";

    if (!m_activeCallPath.isEmpty()) {
        // Clear state immediately for responsive UI
        m_hasActiveCall = false;
        QString callPath = m_activeCallPath;
        m_activeCallPath.clear();
        m_activeCallNumber.clear();
        m_activeCallName.clear();
        m_activeCallState.clear();
        m_activeCallDuration = 0;
        m_callDurationTimer->stop();
        emit activeCallChanged();

        QDBusMessage hangupMsg = QDBusMessage::createMethodCall(
            "org.ofono", callPath, "org.ofono.VoiceCall", "Hangup");

        auto pending = QDBusConnection::systemBus().asyncCall(hangupMsg);
        auto *watcher = new QDBusPendingCallWatcher(pending, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<> reply = *w;
            if (reply.isError()) {
                qWarning() << "TelephonyManager: Hangup failed:" << reply.error().message();
            } else {
                qDebug() << "TelephonyManager: Call hung up successfully";
            }
            w->deleteLater();
        });
        return;
    }

    // Fallback: HangupAll — still sync since it's a rare fallback path
    QDBusInterface ofonoManager(
        "org.ofono", "/", "org.ofono.Manager", QDBusConnection::systemBus());

    if (!ofonoManager.isValid()) {
        qWarning() << "TelephonyManager: oFono not available";
        return;
    }

    QDBusReply<QDBusArgument> reply = ofonoManager.call("GetModems");
    if (!reply.isValid()) return;

    QDBusArgument arg = reply.value();
    arg.beginArray();
    QString modemPath;
    while (!arg.atEnd()) {
        arg.beginStructure();
        arg >> modemPath;
        arg.beginMap();
        arg.endMap();
        arg.endStructure();
        break;
    }
    arg.endArray();

    if (!modemPath.isEmpty()) {
        QDBusMessage hangupAllMsg = QDBusMessage::createMethodCall(
            "org.ofono", modemPath, "org.ofono.VoiceCallManager", "HangupAll");
        QDBusConnection::systemBus().asyncCall(hangupAllMsg);
        qDebug() << "TelephonyManager: HangupAll sent";
    }
#endif
}

void TelephonyManager::toggleMute()
{
#ifdef Q_OS_WIN
    qDebug() << "TelephonyManager: Mock toggle mute";
    m_isCallMuted = !m_isCallMuted;
    emit callMutedChanged();
#else
    if (m_activeCallPath.isEmpty()) {
        qWarning() << "TelephonyManager: No active call to mute/unmute";
        return;
    }

    qDebug() << "TelephonyManager: Toggling mute for call:" << m_activeCallPath;

    QDBusInterface voiceCall(
        "org.ofono",
        m_activeCallPath,
        "org.freedesktop.DBus.Properties",
        QDBusConnection::systemBus()
    );

    if (!voiceCall.isValid()) {
        qWarning() << "TelephonyManager: VoiceCall properties interface not available";
        m_isCallMuted = !m_isCallMuted;
        emit callMutedChanged();
        return;
    }

    QDBusReply<QVariant> muteReply = voiceCall.call("Get", "org.ofono.VoiceCall", "Muted");
    if (!muteReply.isValid()) {
        qWarning() << "TelephonyManager: Failed to get mute state:" << muteReply.error().message();
        m_isCallMuted = !m_isCallMuted;
        emit callMutedChanged();
        return;
    }

    bool currentMuteState = muteReply.value().toBool();
    bool newMuteState = !currentMuteState;

    QDBusReply<void> setReply = voiceCall.call("Set", "org.ofono.VoiceCall", "Muted", QVariant::fromValue(QDBusVariant(newMuteState)));
    if (setReply.isValid()) {
        m_isCallMuted = newMuteState;
        emit callMutedChanged();
        qDebug() << "TelephonyManager: Call" << (newMuteState ? "muted" : "unmuted");
    } else {
        qWarning() << "TelephonyManager: Failed to toggle mute:" << setReply.error().message();
        m_isCallMuted = !m_isCallMuted;
        emit callMutedChanged();
    }
#endif
}

void TelephonyManager::sendDTMF(const QString &tones)
{
#ifdef Q_OS_WIN
    qDebug() << "TelephonyManager: Mock send DTMF:" << tones;
#else
    if (tones.isEmpty()) {
        return;
    }

    qDebug() << "TelephonyManager: Sending DTMF tones:" << tones;

    QDBusInterface ofonoManager(
        "org.ofono",
        "/",
        "org.ofono.Manager",
        QDBusConnection::systemBus()
    );

    if (!ofonoManager.isValid()) {
        qWarning() << "TelephonyManager: oFono not available";
        return;
    }

    QDBusReply<QDBusArgument> reply = ofonoManager.call("GetModems");
    if (!reply.isValid()) {
        return;
    }

    QDBusArgument arg = reply.value();
    arg.beginArray();

    QString modemPath;
    while (!arg.atEnd()) {
        arg.beginStructure();
        arg >> modemPath;
        arg.beginMap();
        arg.endMap();
        arg.endStructure();
        break;
    }
    arg.endArray();

    if (modemPath.isEmpty()) {
        return;
    }

    QDBusInterface vcm(
        "org.ofono",
        modemPath,
        "org.ofono.VoiceCallManager",
        QDBusConnection::systemBus()
    );

    if (vcm.isValid()) {
        QDBusReply<void> dtmfReply = vcm.call("SendTones", tones);
        if (dtmfReply.isValid()) {
            qDebug() << "TelephonyManager: DTMF tones sent successfully";
        } else {
            qWarning() << "TelephonyManager: SendTones failed:" << dtmfReply.error().message();
        }
    }
#endif
}

// ========================================================================
// CALL STATE MONITORING
// ========================================================================

#ifndef Q_OS_WIN

void TelephonyManager::onCallPropertyChanged(const QString &propertyName, const QDBusVariant &value)
{
    qDebug() << "TelephonyManager: Call property changed:" << propertyName << "=" << value.variant();

    if (propertyName == "State") {
        QString newState = value.variant().toString();
        m_activeCallState = newState;
        qDebug() << "TelephonyManager: Call state changed to:" << newState;

        if (newState == "active" && !m_callDurationTimer->isActive()) {
            m_callStartTime = QDateTime::currentDateTime();
            m_activeCallDuration = 0;
            m_callDurationTimer->start();
            qDebug() << "TelephonyManager: Call answered, starting timer";
        }

        if (newState == "disconnected") {
            qDebug() << "TelephonyManager: Call disconnected, clearing state";
            m_hasActiveCall = false;
            m_activeCallPath.clear();
            m_activeCallNumber.clear();
            m_activeCallName.clear();
            m_activeCallState.clear();
            m_activeCallDuration = 0;
            m_callDurationTimer->stop();
        }

        emit activeCallChanged();
    }

    if (propertyName == "LineIdentification") {
        QString callerName = value.variant().toString();
        if (!callerName.isEmpty()) {
            m_activeCallName = callerName;
            emit activeCallChanged();
        }
    }
}

void TelephonyManager::onCallAdded(const QDBusObjectPath &path, const QVariantMap &properties)
{
    QString callPath = path.path();
    qDebug() << "TelephonyManager: Call added:" << callPath;
    qDebug() << "TelephonyManager: Call properties:" << properties;

    QString state = properties.value("State").toString();
    QString lineId = properties.value("LineIdentification").toString();

    m_activeCallPath = callPath;
    m_activeCallNumber = lineId;
    m_activeCallName = "";
    m_activeCallState = state;
    m_activeCallDuration = 0;
    m_callStartTime = QDateTime::currentDateTime();
    m_hasActiveCall = true;

    // Try to lookup contact name
    if (m_contactManager) {
        QString contactName = m_contactManager->findContactNameByNumber(lineId);
        if (!contactName.isEmpty()) {
            m_activeCallName = contactName;
            qDebug() << "TelephonyManager: Resolved contact name:" << contactName;
        }
    }

    // Subscribe to property changes for this call
    QDBusConnection::systemBus().connect(
        "org.ofono",
        callPath,
        "org.ofono.VoiceCall",
        "PropertyChanged",
        this,
        SLOT(onCallPropertyChanged(QString,QDBusVariant))
    );

    emit activeCallChanged();

    if (state == "incoming") {
        qDebug() << "TelephonyManager: INCOMING CALL from" << lineId;
    } else if (state == "dialing") {
        qDebug() << "TelephonyManager: Dialing" << lineId;
    }
}

void TelephonyManager::onCallRemoved(const QDBusObjectPath &path)
{
    QString callPath = path.path();
    qDebug() << "TelephonyManager: Call removed:" << callPath;

    if (m_activeCallPath == callPath) {
        qDebug() << "TelephonyManager: Active call ended";
        m_hasActiveCall = false;
        m_activeCallPath.clear();
        m_activeCallNumber.clear();
        m_activeCallName.clear();
        m_activeCallState.clear();
        m_activeCallDuration = 0;
        m_callDurationTimer->stop();

        emit activeCallChanged();
    }
}

#endif // Q_OS_WIN

void TelephonyManager::updateCallDuration()
{
    if (m_hasActiveCall) {
        m_activeCallDuration = m_callStartTime.secsTo(QDateTime::currentDateTime());
        emit activeCallChanged();
    }
}
