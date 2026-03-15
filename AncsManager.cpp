#include "AncsManager.h"
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QRegularExpression>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDBusMetaType>
#include <QtEndian>
#endif

// ANCS UUIDs (lowercase, no braces — matches BlueZ D-Bus representation)
const QString AncsManager::ANCS_SERVICE_UUID     = "7905f431-b5ce-4e99-a40f-4b1e122d00d0";
const QString AncsManager::NOTIFICATION_SOURCE_UUID = "9fbf120d-6301-42d9-8c58-25e699a21dbd";
const QString AncsManager::CONTROL_POINT_UUID    = "69d1d8f3-45e1-49a8-9821-9bbdfdaad9d9";
const QString AncsManager::DATA_SOURCE_UUID      = "22eac6e9-24d6-4bb5-be44-b36ace7c7bfb";

// D-Bus advertisement object path (registered with BlueZ)
static const QString ADV_PATH = "/com/headunit/ancs/advertisement0";

AncsManager::AncsManager(QObject *parent)
    : QObject(parent)
    , m_advertising(false)
    , m_connected(false)
    , m_advertisementRegistered(false)
    , m_classicConnected(false)
    , m_discoveryTimer(new QTimer(this))
{
#ifndef Q_OS_WIN
    // Periodically scan for ANCS GATT characteristics after a device bonds
    m_discoveryTimer->setInterval(5000);
    connect(m_discoveryTimer, &QTimer::timeout, this, [this]() {
        checkForAncsCharacteristics();

        // Clean up stale pending notifications (older than 30 seconds)
        auto it = m_pendingNotifications.begin();
        while (it != m_pendingNotifications.end()) {
            QDateTime ts = QDateTime::fromString(it.value()["timestamp"].toString(), Qt::ISODate);
            if (ts.isValid() && ts.secsTo(QDateTime::currentDateTime()) > 30) {
                // Emit with basic info since attributes never arrived
                QVariantMap n = it.value();
                n["appName"] = n.value("categoryName", "Notification").toString();
                n["title"] = "Notification";
                n["message"] = "";
                emit notificationReceived(n);
                it = m_pendingNotifications.erase(it);
            } else {
                ++it;
            }
        }
    });

    // Watch for new GATT characteristics appearing on D-Bus
    QDBusConnection::systemBus().connect(
        "org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
        this, SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap))
    );

    QDBusConnection::systemBus().connect(
        "org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList))
    );

    qDebug() << "AncsManager: Initialized, ready to advertise";
#endif
}

AncsManager::~AncsManager()
{
#ifndef Q_OS_WIN
    // Disconnect D-Bus signal subscriptions
    QDBusConnection::systemBus().disconnect(
        "org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
        this, SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap))
    );
    QDBusConnection::systemBus().disconnect(
        "org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList))
    );

    // Disconnect per-characteristic signal subscriptions
    if (!m_notificationSourcePath.isEmpty()) {
        QDBusConnection::systemBus().disconnect(
            "org.bluez", m_notificationSourcePath,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            this, SLOT(onNotificationSourceChanged(QString,QVariantMap,QStringList))
        );
    }
    if (!m_dataSourcePath.isEmpty()) {
        QDBusConnection::systemBus().disconnect(
            "org.bluez", m_dataSourcePath,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            this, SLOT(onDataSourceChanged(QString,QVariantMap,QStringList))
        );
    }

    stopAdvertising();
#endif
}

// ============================================================================
// BLE ADVERTISEMENT
// ============================================================================

void AncsManager::startAdvertising()
{
#ifndef Q_OS_WIN
    if (m_advertisementRegistered) {
        qDebug() << "AncsManager: Already advertising";
        return;
    }

    // Start looking for ANCS characteristics immediately
    // (they may already exist from a previous connection)
    m_discoveryTimer->start();
    checkForAncsCharacteristics();

    if (!registerAdvertisement()) {
        // The async registration might still succeed — the timer will keep scanning
        qWarning() << "AncsManager: Advertisement registration pending (async)";
    }

    m_advertising = true;
    emit advertisingChanged();

    qDebug() << "AncsManager: BLE ANCS service discovery started";
#endif
}

void AncsManager::stopAdvertising()
{
#ifndef Q_OS_WIN
    if (m_advertisementRegistered) {
        unregisterAdvertisement();
    }
    m_discoveryTimer->stop();
    m_advertising = false;
    emit advertisingChanged();
#endif
}

#ifndef Q_OS_WIN

// ============================================================================
// D-BUS ADVERTISEMENT REGISTRATION
// ============================================================================

bool AncsManager::registerAdvertisement()
{
    // We need to expose an object on D-Bus that implements org.bluez.LEAdvertisement1.
    // BlueZ reads properties from this object to build the advertising data.
    //
    // Required properties:
    //   Type: "peripheral"
    //   SolicitUUIDs: ["7905f431-b5ce-4e99-a40f-4b1e122d00d0"]  (ANCS)
    //   LocalName: "TruckHeadUnit"
    //   Includes: ["tx-power"]
    //
    // We register this as a simple D-Bus object using QDBusConnection.

    // Register the advertisement object on D-Bus
    // BlueZ will call GetAll on org.bluez.LEAdvertisement1 and
    // org.freedesktop.DBus.Properties to read our advertisement data.

    // For BlueZ D-Bus advertisement, we use a helper approach:
    // Register via bluetoothctl-style commands through the management interface.
    // This is more reliable than implementing a full D-Bus object server.

    // Use the LEAdvertisingManager1 directly with a registered object.
    // Since implementing a full D-Bus object server in C++ is verbose,
    // we'll use a small script approach via QProcess, or use the
    // org.bluez.LEAdvertisingManager1 with a properly registered object.

    // Actually, let's register the D-Bus object properly using QDBusConnection.
    // We need to handle GetAll and Get calls for the LEAdvertisement1 interface.

    // Step 1: Register our advertisement object on the system bus
    bool registered = QDBusConnection::systemBus().registerObject(
        ADV_PATH, this,
        QDBusConnection::ExportScriptableContents | QDBusConnection::ExportAdaptors);

    // The proper way is to use a QDBusAbstractAdaptor. But for simplicity,
    // let's use the btmgmt / bluetoothctl approach via D-Bus method calls.
    // Unfortunately LEAdvertisingManager1.RegisterAdvertisement() requires
    // the advertisement object to respond to D-Bus property queries.

    // Simpler approach: use hcitool or btmgmt for advertising data,
    // or use a small Python helper script.

    // Most robust approach for our C++ app: write the advertisement
    // using QDBus adaptors properly.

    // For now, let's use the proven approach from the BlueZ test scripts:
    // Register a proper D-Bus object that responds to property queries.

    // We'll do this by connecting to the session bus and using QDBus
    // to register an adaptor. But since LEAdvertisement needs the system bus
    // and our app runs as a regular user, we use the system bus.

    // Let's use a direct approach: call btmgmt to add the advertisement
    // This is the most reliable method on embedded systems.

    // Actually, the cleanest approach is to register our own D-Bus object.
    // Let me implement a minimal version.

    // Unregister any previous attempt
    QDBusConnection::systemBus().unregisterObject(ADV_PATH);

    // We'll handle the D-Bus property requests in a custom handler.
    // For now, let's use the simplest working approach: hciconfig + hcitool.

    // The most portable approach: use bluetoothctl's advertise command
    // programmatically. But that requires an interactive session.

    // Best approach for production: register a DBus object adaptor.
    // Since this is complex, let's start with a Python helper that
    // runs as a subprocess, similar to how tidal_service.py works.
    // This is what ancs4linux does too.

    // BUT — there's actually a much simpler way on BlueZ 5.48+:
    // Use the experimental btmgmt add-adv command or use
    // org.bluez.LEAdvertisingManager1 with a minimal D-Bus service.

    // Let's use the D-Bus approach properly with QDBusVirtualObject.

    qDebug() << "AncsManager: Registering BLE advertisement via D-Bus helper";

    // Write a small Python script inline and run it as a background process
    // This is the approach ancs4linux uses and it's proven to work.
    QString script = R"PY(
import dbus, dbus.service, dbus.mainloop.glib
from gi.repository import GLib
import sys

ANCS_UUID = '7905f431-b5ce-4e99-a40f-4b1e122d00d0'
ADV_PATH = '/com/headunit/ancs/advertisement0'
AGENT_PATH = '/com/headunit/ancs/agent0'

# ── BLE Advertisement ──
class Advertisement(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, ADV_PATH)

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ss', out_signature='v')
    def Get(self, interface, prop):
        return self.GetAll(interface)[prop]

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != 'org.bluez.LEAdvertisement1':
            return {}
        return {
            'Type': dbus.String('peripheral'),
            'SolicitUUIDs': dbus.Array([ANCS_UUID], signature='s'),
            'LocalName': dbus.String('TruckHeadUnit'),
            'Includes': dbus.Array(['tx-power'], signature='s'),
        }

    @dbus.service.method('org.bluez.LEAdvertisement1', in_signature='', out_signature='')
    def Release(self):
        pass

# ── Pairing Agent (numeric comparison) ──
class PairingAgent(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, AGENT_PATH)

    @dbus.service.method('org.bluez.Agent1', in_signature='os', out_signature='')
    def RequestConfirmation(self, device, passkey):
        # Numeric comparison — auto-confirm on head unit side.
        # The iPhone will show the same passkey and ask the user to tap "Pair".
        print(f'PAIRING_CONFIRM:{passkey}:{device}', flush=True)
        # Return without error = confirmation accepted on this side
        return

    @dbus.service.method('org.bluez.Agent1', in_signature='o', out_signature='')
    def RequestAuthorization(self, device):
        print(f'PAIRING_AUTH:{device}', flush=True)
        return

    @dbus.service.method('org.bluez.Agent1', in_signature='os', out_signature='')
    def AuthorizeService(self, device, uuid):
        print(f'SERVICE_AUTH:{uuid}:{device}', flush=True)
        return

    @dbus.service.method('org.bluez.Agent1', in_signature='', out_signature='')
    def Release(self):
        pass

    @dbus.service.method('org.bluez.Agent1', in_signature='', out_signature='')
    def Cancel(self):
        pass

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SystemBus()

# Register advertisement
adv = Advertisement(bus)

# Register pairing agent with DisplayYesNo capability (numeric comparison)
agent = PairingAgent(bus)
agent_mgr = dbus.Interface(bus.get_object('org.bluez', '/org/bluez'),
                           'org.bluez.AgentManager1')
try:
    agent_mgr.RegisterAgent(AGENT_PATH, 'DisplayYesNo')
    agent_mgr.RequestDefaultAgent(AGENT_PATH)
    print('AGENT_REGISTERED', flush=True)
except Exception as e:
    print(f'AGENT_ERROR: {e}', flush=True, file=sys.stderr)

# Ensure adapter is pairable and discoverable
props = dbus.Interface(bus.get_object('org.bluez', '/org/bluez/hci0'),
                       'org.freedesktop.DBus.Properties')
props.Set('org.bluez.Adapter1', 'Pairable', dbus.Boolean(True))
props.Set('org.bluez.Adapter1', 'Discoverable', dbus.Boolean(True))

# Register BLE advertisement (async)
adv_mgr = dbus.Interface(bus.get_object('org.bluez', '/org/bluez/hci0'),
                         'org.bluez.LEAdvertisingManager1')

def on_registered():
    print('ANCS_ADV_READY', flush=True)

def on_error(e):
    print(f'ANCS_ADV_ERROR: {e}', flush=True, file=sys.stderr)

adv_mgr.RegisterAdvertisement(ADV_PATH, {},
    reply_handler=on_registered,
    error_handler=on_error)

GLib.MainLoop().run()
)PY";

    // Write script to temp file and run
    QFile scriptFile("/tmp/ancs_advertise.py");
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        scriptFile.write(script.toUtf8());
        scriptFile.close();
    }

    m_advProcess = new QProcess(this);
    connect(m_advProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QString output = m_advProcess->readAllStandardOutput();
        for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
            if (line.contains("ANCS_ADV_READY")) {
                m_advertisementRegistered = true;
                qDebug() << "AncsManager: BLE peripheral advertisement registered with BlueZ";
            } else if (line.startsWith("AGENT_REGISTERED")) {
                qDebug() << "AncsManager: Pairing agent registered (DisplayYesNo)";
            } else if (line.startsWith("PAIRING_CONFIRM:")) {
                // Numeric comparison — log the passkey (auto-confirmed on our side)
                QStringList parts = line.split(':');
                QString passkey = parts.value(1);
                QString device = parts.value(2);
                qDebug() << "AncsManager: Pairing confirmation — passkey:" << passkey << "device:" << device;
            } else if (line.startsWith("PAIRING_AUTH:")) {
                qDebug() << "AncsManager: Pairing authorization for" << line.mid(13);
            } else if (line.startsWith("SERVICE_AUTH:")) {
                qDebug() << "AncsManager: Service authorization —" << line.mid(13);
            }
        }
    });
    connect(m_advProcess, &QProcess::readyReadStandardError, this, [this]() {
        QString err = m_advProcess->readAllStandardError();
        if (!err.trimmed().isEmpty()) {
            qWarning() << "AncsManager: Advertisement helper error:" << err.trimmed();
        }
    });
    connect(m_advProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
                qDebug() << "AncsManager: Advertisement helper exited with code" << code;
                m_advertisementRegistered = false;
                m_advertising = false;
                emit advertisingChanged();
            });

    m_advProcess->start("python3", {"/tmp/ancs_advertise.py"});

    if (!m_advProcess->waitForStarted(3000)) {
        qWarning() << "AncsManager: Failed to start advertisement helper";
        return false;
    }

    // Wait briefly for the READY signal
    m_advProcess->waitForReadyRead(3000);
    return m_advertisementRegistered;
}

void AncsManager::unregisterAdvertisement()
{
    if (m_advProcess) {
        m_advProcess->terminate();
        m_advProcess->waitForFinished(2000);
        if (m_advProcess->state() != QProcess::NotRunning) {
            m_advProcess->kill();
        }
        m_advProcess->deleteLater();
        m_advProcess = nullptr;
    }
    m_advertisementRegistered = false;
}

// ============================================================================
// GATT CHARACTERISTIC DISCOVERY
// ============================================================================

void AncsManager::checkForAncsCharacteristics()
{
    // If already found all characteristics, nothing to do
    if (m_connected) return;

    // Use Introspect to recursively find GATT characteristic paths,
    // then query each for its UUID via D-Bus Properties.Get.
    // BlueZ paths: /org/bluez/hci0/dev_XX_XX/serviceXXXX/charXXXX

    // Get a flat list of GATT characteristic object paths using
    // busctl or direct introspection of known device paths
    QStringList charPaths;

    // Introspect the BlueZ root to find all device paths
    QDBusInterface rootIntrospect("org.bluez", "/org/bluez/hci0",
                                   "org.freedesktop.DBus.Introspectable",
                                   QDBusConnection::systemBus());
    QDBusReply<QString> rootXml = rootIntrospect.call("Introspect");
    if (!rootXml.isValid()) return;

    // Find device nodes (dev_XX_XX_XX_XX_XX_XX)
    QRegularExpression devRe("name=\"(dev_[^\"]+)\"");
    auto devIt = devRe.globalMatch(rootXml.value());
    while (devIt.hasNext()) {
        QString devName = devIt.next().captured(1);
        QString devPath = "/org/bluez/hci0/" + devName;

        // Introspect device to find service nodes
        QDBusInterface devIntrospect("org.bluez", devPath,
                                      "org.freedesktop.DBus.Introspectable",
                                      QDBusConnection::systemBus());
        QDBusReply<QString> devXml = devIntrospect.call("Introspect");
        if (!devXml.isValid()) continue;

        QRegularExpression svcRe("name=\"(service[^\"]+)\"");
        auto svcIt = svcRe.globalMatch(devXml.value());
        while (svcIt.hasNext()) {
            QString svcName = svcIt.next().captured(1);
            QString svcPath = devPath + "/" + svcName;

            // Introspect service to find characteristic nodes
            QDBusInterface svcIntrospect("org.bluez", svcPath,
                                          "org.freedesktop.DBus.Introspectable",
                                          QDBusConnection::systemBus());
            QDBusReply<QString> svcXml = svcIntrospect.call("Introspect");
            if (!svcXml.isValid()) continue;

            QRegularExpression charRe("name=\"(char[^\"]+)\"");
            auto charIt = charRe.globalMatch(svcXml.value());
            while (charIt.hasNext()) {
                QString charName = charIt.next().captured(1);
                charPaths.append(svcPath + "/" + charName);
            }
        }
    }

    if (charPaths.isEmpty()) return;

    // Query each characteristic's UUID
    for (const QString &path : charPaths) {
        QDBusInterface propIface("org.bluez", path,
                                 "org.freedesktop.DBus.Properties",
                                 QDBusConnection::systemBus());

        QDBusMessage uuidReply = propIface.call("Get", "org.bluez.GattCharacteristic1", "UUID");
        if (uuidReply.type() != QDBusMessage::ReplyMessage) continue;

        QString uuid = uuidReply.arguments().at(0).value<QDBusVariant>().variant().toString();

        if (uuid == NOTIFICATION_SOURCE_UUID && m_notificationSourcePath.isEmpty()) {
            m_notificationSourcePath = path;
            qDebug() << "AncsManager: Found Notification Source at" << path;
            subscribeToCharacteristic(path);
        } else if (uuid == DATA_SOURCE_UUID && m_dataSourcePath.isEmpty()) {
            m_dataSourcePath = path;
            qDebug() << "AncsManager: Found Data Source at" << path;
            subscribeToCharacteristic(path);
        } else if (uuid == CONTROL_POINT_UUID && m_controlPointPath.isEmpty()) {
            m_controlPointPath = path;
            qDebug() << "AncsManager: Found Control Point at" << path;
        }
    }

    // If all three characteristics found, we're fully connected
    if (!m_notificationSourcePath.isEmpty() && !m_dataSourcePath.isEmpty() && !m_controlPointPath.isEmpty()) {
        if (!m_connected) {
            m_connected = true;
            emit connectedChanged();
            qDebug() << "AncsManager: ANCS fully connected — all 3 characteristics found";
            m_discoveryTimer->stop();
        }
    }
}

void AncsManager::subscribeToCharacteristic(const QString &charPath)
{
    // Call StartNotify() on the characteristic
    QDBusInterface charIface("org.bluez", charPath,
                             "org.bluez.GattCharacteristic1",
                             QDBusConnection::systemBus());

    if (!charIface.isValid()) {
        qWarning() << "AncsManager: Invalid characteristic interface at" << charPath;
        return;
    }

    QDBusMessage reply = charIface.call("StartNotify");
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "AncsManager: StartNotify failed on" << charPath << ":" << reply.errorMessage();
        return;
    }

    qDebug() << "AncsManager: Subscribed to notifications on" << charPath;

    // Monitor PropertiesChanged — use path-specific slots to distinguish characteristics
    if (charPath == m_notificationSourcePath) {
        QDBusConnection::systemBus().connect(
            "org.bluez", charPath,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            this, SLOT(onNotificationSourceChanged(QString,QVariantMap,QStringList))
        );
    } else if (charPath == m_dataSourcePath) {
        QDBusConnection::systemBus().connect(
            "org.bluez", charPath,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            this, SLOT(onDataSourceChanged(QString,QVariantMap,QStringList))
        );
    }
}

// ============================================================================
// D-BUS SIGNAL HANDLERS
// ============================================================================

void AncsManager::onInterfacesAdded(const QDBusObjectPath &path, const QVariantMap &interfaces)
{
    // Check if a new GATT characteristic appeared matching ANCS
    if (!interfaces.contains("org.bluez.GattCharacteristic1")) return;

    // Get the UUID of this characteristic
    QDBusInterface charIface("org.bluez", path.path(),
                             "org.freedesktop.DBus.Properties",
                             QDBusConnection::systemBus());

    QDBusMessage reply = charIface.call("Get", "org.bluez.GattCharacteristic1", "UUID");
    if (reply.type() != QDBusMessage::ReplyMessage) return;

    QString uuid = reply.arguments().at(0).value<QDBusVariant>().variant().toString();
    QString p = path.path();

    if (uuid == NOTIFICATION_SOURCE_UUID) {
        m_notificationSourcePath = p;
        qDebug() << "AncsManager: Notification Source appeared at" << p;
        subscribeToCharacteristic(p);
    } else if (uuid == DATA_SOURCE_UUID) {
        m_dataSourcePath = p;
        qDebug() << "AncsManager: Data Source appeared at" << p;
        subscribeToCharacteristic(p);
    } else if (uuid == CONTROL_POINT_UUID) {
        m_controlPointPath = p;
        qDebug() << "AncsManager: Control Point appeared at" << p;
    }

    // Check if we're fully connected
    if (!m_notificationSourcePath.isEmpty() && !m_dataSourcePath.isEmpty() && !m_controlPointPath.isEmpty()) {
        if (!m_connected) {
            m_connected = true;
            emit connectedChanged();
            qDebug() << "AncsManager: ANCS fully connected";
            m_discoveryTimer->stop();

            // Extract device path from characteristic path and connect BR/EDR profiles
            // Path: /org/bluez/hci0/dev_XX_XX/serviceXXXX/charXXXX → /org/bluez/hci0/dev_XX_XX
            QString devPath = p.section("/service", 0, 0);
            if (!devPath.isEmpty() && devPath != m_bondedDevicePath) {
                m_bondedDevicePath = devPath;
                m_classicConnected = false;
                // Delay BR/EDR connect slightly to let LE bond stabilize
                QTimer::singleShot(2000, this, [this]() {
                    connectClassicProfiles(m_bondedDevicePath);
                });
            }
        }
    }
}

void AncsManager::onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces)
{
    if (!interfaces.contains("org.bluez.GattCharacteristic1")) return;

    QString p = path.path();
    if (p == m_notificationSourcePath) m_notificationSourcePath.clear();
    else if (p == m_dataSourcePath) m_dataSourcePath.clear();
    else if (p == m_controlPointPath) m_controlPointPath.clear();
    else return;

    if (m_connected) {
        m_connected = false;
        m_classicConnected = false;
        emit connectedChanged();
        qDebug() << "AncsManager: ANCS characteristic removed, disconnected";
        m_discoveryTimer->start(); // Resume polling
    }
}

void AncsManager::onNotificationSourceChanged(const QString &interface,
                                                const QVariantMap &changed,
                                                const QStringList &invalidated)
{
    Q_UNUSED(invalidated);
    if (interface != "org.bluez.GattCharacteristic1") return;
    if (!changed.contains("Value")) return;
    handleNotificationSourceValue(changed.value("Value").toByteArray());
}

void AncsManager::onDataSourceChanged(const QString &interface,
                                       const QVariantMap &changed,
                                       const QStringList &invalidated)
{
    Q_UNUSED(invalidated);
    if (interface != "org.bluez.GattCharacteristic1") return;
    if (!changed.contains("Value")) return;
    QByteArray value = changed.value("Value").toByteArray();
    qDebug() << "AncsManager: Data Source received" << value.size() << "bytes";
    handleDataSourceValue(value);
}

// ============================================================================
// BR/EDR CLASSIC PROFILE CONNECTION (after LE bond)
// ============================================================================

QString AncsManager::extractDeviceAddress(const QString &charPath)
{
    // Path format: /org/bluez/hci0/dev_80_96_98_C8_69_17/serviceXXXX/charXXXX
    QRegularExpression re("(dev_([0-9A-F_]+))", QRegularExpression::CaseInsensitiveOption);
    auto match = re.match(charPath);
    if (match.hasMatch()) {
        return match.captured(2).replace("_", ":");
    }
    return QString();
}

void AncsManager::connectClassicProfiles(const QString &devicePath)
{
    if (m_classicConnected) return;

    qDebug() << "AncsManager: LE bond established — connecting BR/EDR profiles on" << devicePath;

    // Trust the device so BR/EDR auto-reconnects in the future
    QDBusInterface propIface("org.bluez", devicePath,
                             "org.freedesktop.DBus.Properties",
                             QDBusConnection::systemBus());
    propIface.call("Set", "org.bluez.Device1", "Trusted",
                   QVariant::fromValue(QDBusVariant(true)));

    // Connect classic profiles: HFP (111e), A2DP Sink (110b), AVRCP (110e)
    QDBusInterface devIface("org.bluez", devicePath,
                            "org.bluez.Device1",
                            QDBusConnection::systemBus());

    // Use Connect() which connects all available profiles
    QDBusMessage reply = devIface.call("Connect");
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "AncsManager: BR/EDR Connect failed:" << reply.errorMessage()
                    << "— trying individual profiles";

        // Fall back to connecting individual profiles
        static const QStringList profiles = {
            "0000111e-0000-1000-8000-00805f9b34fb",  // HFP
            "0000110b-0000-1000-8000-00805f9b34fb",  // A2DP Sink
            "0000110e-0000-1000-8000-00805f9b34fb",  // AVRCP
        };
        for (const QString &uuid : profiles) {
            QDBusMessage profReply = devIface.call("ConnectProfile", uuid);
            if (profReply.type() == QDBusMessage::ErrorMessage) {
                qWarning() << "AncsManager: ConnectProfile" << uuid << "failed:" << profReply.errorMessage();
            } else {
                qDebug() << "AncsManager: Connected profile" << uuid;
            }
        }
    } else {
        qDebug() << "AncsManager: BR/EDR Connect() succeeded — all profiles connecting";
    }

    m_classicConnected = true;

    // Extract address and signal to the rest of the system
    QString address = extractDeviceAddress(devicePath);
    if (!address.isEmpty()) {
        emit deviceBondedOverLE(address);
    }
}

// ============================================================================
// NOTIFICATION PROCESSING
// ============================================================================

void AncsManager::handleNotificationSourceValue(const QByteArray &data)
{
    if (data.size() < 8) return;

    quint8 eventId = static_cast<quint8>(data[0]);
    quint8 eventFlags = static_cast<quint8>(data[1]);
    quint8 categoryId = static_cast<quint8>(data[2]);
    quint32 uid;
    memcpy(&uid, data.constData() + 4, 4); // Little-endian on wire

    QString notifId = QString("ancs_%1").arg(uid);

    if (eventId == 2) {
        // Removed
        emit notificationRemoved(notifId);
        m_pendingNotifications.remove(uid);
        return;
    }

    if (eventId == 0) {
        // Added — skip pre-existing notifications (bit 2 of eventFlags)
        bool preExisting = (eventFlags & 0x04);
        if (preExisting) return;  // Don't flood requests for old notifications

        // Request full attributes for new notifications
        static const QStringList categoryNames = {
            "Other", "Incoming Call", "Missed Call", "Voicemail",
            "Social", "Schedule", "Email", "News",
            "Health & Fitness", "Business & Finance", "Location", "Entertainment"
        };

        QVariantMap notification;
        notification["id"] = notifId;
        notification["category"] = (int)categoryId;
        notification["categoryName"] = (categoryId < categoryNames.size()) ? categoryNames[categoryId] : "Other";
        notification["priority"] = (eventFlags & 0x02) ? 3 : 1; // Urgent=3, Normal=1
        notification["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        notification["read"] = false;

        m_pendingNotifications[uid] = notification;
        requestNotificationAttributes(uid);
    }
}

void AncsManager::requestNotificationAttributes(quint32 uid)
{
    if (m_controlPointPath.isEmpty()) {
        // No control point — emit with basic info
        if (m_pendingNotifications.contains(uid)) {
            QVariantMap n = m_pendingNotifications.take(uid);
            n["appName"] = n.value("categoryName", "Notification");
            n["title"] = "New Notification";
            n["message"] = "";
            emit notificationReceived(n);
        }
        return;
    }

    // Build GetNotificationAttributes command
    QByteArray cmd;
    cmd.append(char(0)); // CommandID = 0 (GetNotificationAttributes)
    cmd.append(reinterpret_cast<const char*>(&uid), 4); // UID (little-endian)

    // Attribute 0: AppIdentifier (no max length — null-terminated)
    cmd.append(char(0));

    // Attribute 1: Title (max 64 bytes)
    cmd.append(char(1));
    quint16 maxLen = 64;
    cmd.append(reinterpret_cast<const char*>(&maxLen), 2);

    // Attribute 3: Message (max 256 bytes)
    cmd.append(char(3));
    maxLen = 256;
    cmd.append(reinterpret_cast<const char*>(&maxLen), 2);

    // Write to Control Point via D-Bus
    QDBusInterface cpIface("org.bluez", m_controlPointPath,
                           "org.bluez.GattCharacteristic1",
                           QDBusConnection::systemBus());

    // WriteValue expects an array of bytes and an options dict
    QVariantMap options;
    options["type"] = "command";
    cpIface.call("WriteValue", QVariant::fromValue(cmd), options);

    qDebug() << "AncsManager: Requested attributes for UID" << uid;
}

void AncsManager::handleDataSourceValue(const QByteArray &data)
{
    m_dataBuffer.append(data);

    // Need at least CommandID(1) + UID(4)
    if (m_dataBuffer.size() < 5) return;

    quint8 cmdId = static_cast<quint8>(m_dataBuffer[0]);
    if (cmdId != 0) {
        // Not a GetNotificationAttributes response — discard
        m_dataBuffer.clear();
        return;
    }

    quint32 uid;
    memcpy(&uid, m_dataBuffer.constData() + 1, 4);

    // Parse attributes: each is AttrID(1) + Length(2) + Data(Length)
    int pos = 5;
    QString appId, title, message;
    int attrsFound = 0;

    while (pos < m_dataBuffer.size() && attrsFound < 3) {
        // Need at least the attribute header (3 bytes)
        if (pos + 3 > m_dataBuffer.size()) {
            // Incomplete header — wait for more data
            return;
        }

        quint8 attrId = static_cast<quint8>(m_dataBuffer[pos]);
        quint16 attrLen;
        memcpy(&attrLen, m_dataBuffer.constData() + pos + 1, 2);
        pos += 3;

        // Need the full attribute value
        if (pos + attrLen > m_dataBuffer.size()) {
            // Incomplete attribute data — wait for more data
            return;
        }

        QString attrValue = QString::fromUtf8(m_dataBuffer.mid(pos, attrLen));
        pos += attrLen;
        attrsFound++;

        switch (attrId) {
            case 0: appId = attrValue; break;
            case 1: title = attrValue; break;
            case 3: message = attrValue; break;
        }
    }

    // All 3 attributes parsed successfully — emit and clear buffer
    m_dataBuffer.clear();

    if (m_pendingNotifications.contains(uid)) {
        QVariantMap n = m_pendingNotifications.take(uid);
        n["appId"] = appId;
        n["appName"] = resolveAppName(appId);
        n["title"] = title;
        n["message"] = message;

        qDebug() << "AncsManager:" << n["appName"].toString() << "-" << title << ":" << message;
        emit notificationReceived(n);
    } else {
        // UID not in pending map — might have been a stale response
        qDebug() << "AncsManager: Data Source response for unknown UID" << uid
                 << "app:" << appId << "title:" << title;
    }
}

QString AncsManager::resolveAppName(const QString &bundleId)
{
    static const QMap<QString, QString> names = {
        {"com.apple.MobileSMS", "Messages"},
        {"com.apple.mobilephone", "Phone"},
        {"com.apple.mobilemail", "Mail"},
        {"com.apple.facetime", "FaceTime"},
        {"com.apple.mobilecal", "Calendar"},
        {"com.apple.reminders", "Reminders"},
        {"com.apple.weather", "Weather"},
        {"com.apple.Maps", "Maps"},
        {"com.whatsapp.WhatsApp", "WhatsApp"},
        {"com.facebook.Messenger", "Messenger"},
        {"ph.telegra.Telegraph", "Telegram"},
        {"com.toyopagroup.picaboo", "Snapchat"},
        {"com.google.Gmail", "Gmail"},
        {"com.spotify.client", "Spotify"},
        {"com.slack.Slack", "Slack"},
        {"com.microsoft.teams", "Teams"},
        {"com.atebits.Tweetie2", "Twitter"},
        {"com.burbn.instagram", "Instagram"},
    };

    if (names.contains(bundleId)) return names[bundleId];
    QStringList parts = bundleId.split('.');
    return parts.isEmpty() ? bundleId : parts.last();
}

#endif // Q_OS_WIN
