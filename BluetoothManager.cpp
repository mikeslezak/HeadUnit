#include "BluetoothManager.h"
#include "ContactManager.h"
#include <QDebug>
#include <QRandomGenerator>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

// ========================================================================
// BLUETOOTH MANAGER IMPLEMENTATION
// ========================================================================

BluetoothManager::BluetoothManager(QObject *parent)
    : QObject(parent)
    , m_isScanning(false)
    , m_adapterPowered(false)
    , m_adapterAvailable(false)
    , m_statusMessage("Initializing...")
    , m_adapterPath("/org/bluez/hci0")
    , m_deviceModel(new BluetoothDeviceModel(this))
    , m_telephonyManager(new TelephonyManager(this))
    , m_scanTimer(new QTimer(this))
#ifndef Q_OS_WIN
    , m_adapterInterface(nullptr)
#endif
    , m_contactManager(nullptr)
{
    // Wire up TelephonyManager dependencies
    m_telephonyManager->setAdapterPath(m_adapterPath);
    m_telephonyManager->setDeviceModel(m_deviceModel);

    // Forward telephony signals so QML bindings work unchanged
    connect(m_telephonyManager, &TelephonyManager::activeCallChanged,
            this, &BluetoothManager::activeCallChanged);
    connect(m_telephonyManager, &TelephonyManager::callMutedChanged,
            this, &BluetoothManager::callMutedChanged);
    connect(m_telephonyManager, &TelephonyManager::cellularSignalChanged,
            this, &BluetoothManager::cellularSignalChanged);
    connect(m_telephonyManager, &TelephonyManager::carrierNameChanged,
            this, &BluetoothManager::carrierNameChanged);

#ifdef Q_OS_WIN
    m_mockMode = true;
    qDebug() << "BluetoothManager: Running in MOCK mode (Windows)";
    setStatusMessage("Mock Mode - Simulated Bluetooth");
    m_adapterAvailable = true;
    m_adapterPowered = true;
    emit adapterAvailableChanged();
    emit adapterPoweredChanged();

    QTimer::singleShot(500, this, &BluetoothManager::generateMockDevices);
#else
    m_mockMode = false;
    qDebug() << "BluetoothManager: Real BlueZ mode";
    initialize();
#endif

    // Setup scan timeout timer
    connect(m_scanTimer, &QTimer::timeout, this, &BluetoothManager::onScanTimeout);
    m_scanTimer->setInterval(30000); // 30 second scan timeout
}

BluetoothManager::~BluetoothManager()
{
#ifndef Q_OS_WIN
    if (m_adapterInterface) {
        delete m_adapterInterface;
    }
#endif
}

void BluetoothManager::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "BluetoothManager:" << msg;
    }
}

void BluetoothManager::setContactManager(ContactManager* contactManager)
{
    m_contactManager = contactManager;
    m_telephonyManager->setContactManager(contactManager);
    qDebug() << "BluetoothManager: ContactManager set";
}

// ========================================================================
// INITIALIZATION
// ========================================================================

void BluetoothManager::initialize()
{
#ifndef Q_OS_WIN
    qDebug() << "BluetoothManager: Initializing BlueZ adapter...";

    m_adapterInterface = new QDBusInterface(
        "org.bluez",
        m_adapterPath,
        "org.bluez.Adapter1",
        QDBusConnection::systemBus(),
        this
    );

    if (!m_adapterInterface->isValid()) {
        QString errorMsg = "Bluetooth adapter not available: " +
                          m_adapterInterface->lastError().message();
        qWarning() << "BluetoothManager:" << errorMsg;
        setStatusMessage("Bluetooth not available");
        m_adapterAvailable = false;
        emit adapterAvailableChanged();
        return;
    }

    m_adapterAvailable = true;
    emit adapterAvailableChanged();

    QVariant poweredVar = m_adapterInterface->property("Powered");
    m_adapterPowered = poweredVar.toBool();
    emit adapterPoweredChanged();

    qDebug() << "BluetoothManager: Adapter available, powered:" << m_adapterPowered;

    setupDBusMonitoring();
    loadExistingDevices();

    // Set up oFono monitoring now that adapter is ready
    m_telephonyManager->setupOfonoMonitoring();

    setStatusMessage("Bluetooth ready");
    qDebug() << "BluetoothManager: Initialization complete";
#endif
}

#ifndef Q_OS_WIN

void BluetoothManager::setupDBusMonitoring()
{
    qDebug() << "BluetoothManager: Setting up DBus monitoring...";

    // Monitor for new devices (InterfacesAdded)
    QDBusConnection::systemBus().connect(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "InterfacesAdded",
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap))
    );

    // Monitor for removed devices (InterfacesRemoved)
    QDBusConnection::systemBus().connect(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "InterfacesRemoved",
        this,
        SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList))
    );

    // Monitor adapter property changes
    QDBusConnection::systemBus().connect(
        "org.bluez",
        m_adapterPath,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList))
    );

    qDebug() << "BluetoothManager: DBus monitoring setup complete";
}

void BluetoothManager::loadExistingDevices()
{
    qDebug() << "========================================";
    qDebug() << "BluetoothManager: Loading existing devices...";
    qDebug() << "========================================";

    QDBusInterface manager(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        QDBusConnection::systemBus()
    );

    QDBusMessage response = manager.call("GetManagedObjects");

    if (response.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "BluetoothManager: GetManagedObjects failed:" << response.errorMessage();
        return;
    }

    const QDBusArgument arg = response.arguments().at(0).value<QDBusArgument>();
    arg.beginMap();

    int deviceCount = 0;
    int connectedCount = 0;

    while (!arg.atEnd()) {
        QString objectPath;
        QVariantMap interfaces;

        arg.beginMapEntry();
        arg >> objectPath >> interfaces;
        arg.endMapEntry();

        if (objectPath.startsWith(m_adapterPath + "/dev_") &&
            interfaces.contains("org.bluez.Device1")) {

            qDebug() << "\n--- Found device at path:" << objectPath;

            QVariantMap deviceProps = getDeviceProperties(objectPath);

            if (deviceProps.isEmpty()) {
                qWarning() << "BluetoothManager: Failed to get properties for" << objectPath;
                continue;
            }

            BluetoothDevice device = parseDeviceProperties(objectPath, deviceProps);

            if (interfaces.contains("org.bluez.Battery1")) {
                QVariantMap batteryProps = getBatteryProperties(objectPath);
                if (!batteryProps.isEmpty()) {
                    device.batteryLevel = batteryProps.value("Percentage", -1).toInt();
                    qDebug() << "  Found Battery1 interface - Battery:" << device.batteryLevel << "%";
                }
            }

            m_deviceModel->addDevice(device);
            deviceCount++;

            if (device.connected) {
                connectedCount++;
                qDebug() << "BluetoothManager: Loaded CONNECTED device:" << device.name << "(" << device.address << ")";
            } else if (device.paired) {
                qDebug() << "BluetoothManager: Loaded paired device:" << device.name << "(" << device.address << ")";
            } else {
                qDebug() << "BluetoothManager: Loaded discovered device:" << device.name << "(" << device.address << ")";
            }

            // Monitor this device's properties
            QDBusConnection::systemBus().connect(
                "org.bluez",
                objectPath,
                "org.freedesktop.DBus.Properties",
                "PropertiesChanged",
                this,
                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList))
            );
        }
    }

    arg.endMap();

    qDebug() << "========================================";
    qDebug() << "BluetoothManager: Loaded" << deviceCount << "total devices (" << connectedCount << "connected)";
    qDebug() << "========================================";
    emit deviceCountChanged();

    if (connectedCount > 0) {
        setStatusMessage(QString("%1 device(s) connected").arg(connectedCount));
    } else {
        setStatusMessage("Bluetooth ready");
    }
}

BluetoothDevice BluetoothManager::parseDeviceProperties(const QString &path, const QVariantMap &properties)
{
    BluetoothDevice device;
    device.path = path;
    device.address = pathToAddress(path);

    qDebug() << "  >>> Parsing device properties:";
    qDebug() << "      Path:" << path;
    qDebug() << "      Property count:" << properties.count();
    qDebug() << "      Available properties:" << properties.keys();

    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        qDebug() << "      [" << it.key() << "]" << "=" << it.value() << "(" << it.value().typeName() << ")";
    }

    QString alias = properties.value("Alias").toString();
    QString name = properties.value("Name").toString();
    QString address = properties.value("Address").toString();

    qDebug() << "      Extracted values:";
    qDebug() << "        - Alias:" << (alias.isEmpty() ? "(empty)" : alias);
    qDebug() << "        - Name:" << (name.isEmpty() ? "(empty)" : name);
    qDebug() << "        - Address:" << (address.isEmpty() ? "(empty)" : address);

    if (!alias.isEmpty()) {
        device.name = alias;
        qDebug() << "      Using Alias as device name:" << device.name;
    } else if (!name.isEmpty()) {
        device.name = name;
        qDebug() << "      Using Name as device name:" << device.name;
    } else if (!address.isEmpty()) {
        device.name = address;
        qDebug() << "      Using Address as device name:" << device.name;
    } else {
        device.name = "Unknown Device";
        qDebug() << "      WARNING: No name available, using 'Unknown Device'";
    }

    device.paired = properties.value("Paired", false).toBool();
    device.connected = properties.value("Connected", false).toBool();
    device.trusted = properties.value("Trusted", false).toBool();
    device.signalStrength = properties.value("RSSI", -100).toInt();
    device.batteryLevel = properties.value("Battery", -1).toInt();
    device.icon = properties.value("Icon", "bluetooth").toString();

    qDebug() << "      Final device info:";
    qDebug() << "        - Name:" << device.name;
    qDebug() << "        - Address:" << device.address;
    qDebug() << "        - Paired:" << device.paired;
    qDebug() << "        - Connected:" << device.connected;
    qDebug() << "        - Trusted:" << device.trusted;
    qDebug() << "        - RSSI:" << device.signalStrength;

    return device;
}

QString BluetoothManager::addressToPath(const QString &address)
{
    QString path = address;
    path.replace(":", "_");
    return m_adapterPath + "/dev_" + path;
}

QString BluetoothManager::pathToAddress(const QString &path)
{
    QString devPart = path;
    devPart.remove(0, devPart.lastIndexOf("/dev_") + 5);
    devPart.replace("_", ":");
    return devPart;
}

QVariantMap BluetoothManager::getDeviceProperties(const QString &path)
{
    qDebug() << "  >> Calling GetAll for device:" << path;

    QDBusMessage getAllMsg = QDBusMessage::createMethodCall(
        "org.bluez",
        path,
        "org.freedesktop.DBus.Properties",
        "GetAll"
    );
    getAllMsg << QString("org.bluez.Device1");

    QDBusMessage reply = QDBusConnection::systemBus().call(getAllMsg);

    if (reply.type() != QDBusMessage::ReplyMessage) {
        qWarning() << "BluetoothManager: GetAll failed for" << path << ":" << reply.errorMessage();
        return QVariantMap();
    }

    qDebug() << "  >> GetAll reply arguments count:" << reply.arguments().count();
    if (reply.arguments().isEmpty()) {
        qWarning() << "BluetoothManager: GetAll returned no arguments";
        return QVariantMap();
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

    qDebug() << "  >> GetAll extracted" << properties.count() << "properties";

    return properties;
}

QVariantMap BluetoothManager::getBatteryProperties(const QString &path)
{
    qDebug() << "  >> Calling GetAll for Battery1 interface:" << path;

    QDBusMessage getAllMsg = QDBusMessage::createMethodCall(
        "org.bluez",
        path,
        "org.freedesktop.DBus.Properties",
        "GetAll"
    );
    getAllMsg << QString("org.bluez.Battery1");

    QDBusMessage reply = QDBusConnection::systemBus().call(getAllMsg);

    if (reply.type() != QDBusMessage::ReplyMessage) {
        qDebug() << "  >> Battery1 GetAll failed (interface may not exist):" << reply.errorMessage();
        return QVariantMap();
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
        qDebug() << "  >> Battery property [" << key << "] =" << value;
    }
    arg.endMap();

    return properties;
}

#endif // Q_OS_WIN

// ========================================================================
// SCANNING
// ========================================================================

void BluetoothManager::startScan()
{
#ifdef Q_OS_WIN
    if (m_isScanning) {
        return;
    }

    m_isScanning = true;
    emit scanningChanged();
    setStatusMessage("Mock: Scanning for devices...");

    QTimer::singleShot(2000, this, [this]() {
        BluetoothDevice newDevice;
        newDevice.name = "New Mock Device";
        newDevice.address = QString("AA:BB:CC:DD:EE:%1").arg(QRandomGenerator::global()->bounded(10, 99));
        newDevice.paired = false;
        newDevice.connected = false;
        newDevice.signalStrength = -60;
        newDevice.icon = "phone";

        m_deviceModel->addDevice(newDevice);
        emit deviceCountChanged();
        emit deviceFound(newDevice.address, newDevice.name);
    });

    m_scanTimer->start();
#else
    if (!m_adapterInterface || !m_adapterAvailable) {
        setStatusMessage("Bluetooth adapter not available");
        emit error("Bluetooth adapter not available");
        return;
    }

    if (m_isScanning) {
        qDebug() << "BluetoothManager: Already scanning";
        return;
    }

    qDebug() << "BluetoothManager: Starting device scan...";

    m_isScanning = true;
    emit scanningChanged();
    setStatusMessage("Scanning for devices...");
    m_scanTimer->start();

    auto pending = m_adapterInterface->asyncCall("StartDiscovery");
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            QString errorMsg = "Failed to start scan: " + reply.error().message();
            qWarning() << "BluetoothManager:" << errorMsg;
            m_isScanning = false;
            emit scanningChanged();
            m_scanTimer->stop();
            setStatusMessage("Scan failed");
            emit error(errorMsg);
        } else {
            qDebug() << "BluetoothManager: Scan started";
        }
        w->deleteLater();
    });
#endif
}

void BluetoothManager::stopScan()
{
#ifdef Q_OS_WIN
    m_isScanning = false;
    m_scanTimer->stop();
    emit scanningChanged();
    setStatusMessage("Mock: Scan stopped");
#else
    if (!m_adapterInterface || !m_isScanning) {
        return;
    }

    qDebug() << "BluetoothManager: Stopping scan...";

    m_isScanning = false;
    m_scanTimer->stop();
    emit scanningChanged();
    setStatusMessage("Scan stopped");

    m_adapterInterface->asyncCall("StopDiscovery");
    qDebug() << "BluetoothManager: Scan stopped";
#endif
}

void BluetoothManager::onScanTimeout()
{
    qDebug() << "BluetoothManager: Scan timeout reached";
    stopScan();
    setStatusMessage("Scan complete");
}

// ========================================================================
// DEVICE MANAGEMENT
// ========================================================================

void BluetoothManager::connectToDevice(const QString &address)
{
#ifdef Q_OS_WIN
    setStatusMessage("Mock: Connecting to " + address);

    QTimer::singleShot(1000, this, [this, address]() {
        BluetoothDevice *device = m_deviceModel->findDevice(address);
        if (device) {
            device->connected = true;
            m_deviceModel->updateDevice(address, *device);
            emit deviceConnected(address);
            setStatusMessage("Mock: Connected to " + device->name);
        }
    });
#else
    if (!m_adapterInterface) {
        return;
    }

    qDebug() << "BluetoothManager: Connecting to device:" << address;
    setStatusMessage("Connecting...");

    QString devicePath = addressToPath(address);

    QDBusMessage connectMsg = QDBusMessage::createMethodCall(
        "org.bluez", devicePath, "org.bluez.Device1", "Connect");

    auto pending = QDBusConnection::systemBus().asyncCall(connectMsg);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            QString errorMsg = "Connection failed: " + reply.error().message();
            qWarning() << "BluetoothManager:" << errorMsg;
            emit error(errorMsg);
            setStatusMessage("Connection failed");
        } else {
            qDebug() << "BluetoothManager: Connection initiated";
            setStatusMessage("Connecting...");
            QTimer::singleShot(1000, this, [this]() {
                refreshDeviceList();
            });
        }
        w->deleteLater();
    });
#endif
}

void BluetoothManager::disconnectDevice(const QString &address)
{
#ifdef Q_OS_WIN
    setStatusMessage("Mock: Disconnecting " + address);

    BluetoothDevice *device = m_deviceModel->findDevice(address);
    if (device) {
        device->connected = false;
        m_deviceModel->updateDevice(address, *device);
        emit deviceDisconnected(address);
        setStatusMessage("Mock: Disconnected from " + device->name);
    }
#else
    if (!m_adapterInterface) {
        return;
    }

    qDebug() << "BluetoothManager: Disconnecting device:" << address;

    QString devicePath = addressToPath(address);

    setStatusMessage("Disconnecting...");

    QDBusMessage disconnectMsg = QDBusMessage::createMethodCall(
        "org.bluez", devicePath, "org.bluez.Device1", "Disconnect");

    auto pending = QDBusConnection::systemBus().asyncCall(disconnectMsg);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            QString errorMsg = "Disconnect failed: " + reply.error().message();
            qWarning() << "BluetoothManager:" << errorMsg;
            emit error(errorMsg);
        } else {
            qDebug() << "BluetoothManager: Disconnect initiated";
            QTimer::singleShot(1000, this, [this]() {
                refreshDeviceList();
            });
        }
        w->deleteLater();
    });
#endif
}

void BluetoothManager::pairDevice(const QString &address)
{
#ifdef Q_OS_WIN
    setStatusMessage("Mock: Pairing " + address);

    QTimer::singleShot(1500, this, [this, address]() {
        BluetoothDevice *device = m_deviceModel->findDevice(address);
        if (device) {
            device->paired = true;
            device->trusted = true;
            m_deviceModel->updateDevice(address, *device);
            emit devicePaired(address);
            setStatusMessage("Mock: Paired with " + device->name);
        }
    });
#else
    if (!m_adapterInterface) {
        return;
    }

    qDebug() << "BluetoothManager: Pairing device:" << address;
    setStatusMessage("Pairing...");

    QString devicePath = addressToPath(address);

    QDBusMessage pairMsg = QDBusMessage::createMethodCall(
        "org.bluez", devicePath, "org.bluez.Device1", "Pair");

    auto pending = QDBusConnection::systemBus().asyncCall(pairMsg);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            QString errorMsg = "Pairing failed: " + reply.error().message();
            qWarning() << "BluetoothManager:" << errorMsg;
            emit error(errorMsg);
            setStatusMessage("Pairing failed");
        } else {
            qDebug() << "BluetoothManager: Pairing initiated";
            setStatusMessage("Pairing...");
        }
        w->deleteLater();
    });
#endif
}

void BluetoothManager::unpairDevice(const QString &address)
{
#ifdef Q_OS_WIN
    BluetoothDevice *device = m_deviceModel->findDevice(address);
    if (device) {
        device->paired = false;
        device->trusted = false;
        device->connected = false;
        m_deviceModel->updateDevice(address, *device);
        emit deviceUnpaired(address);
        setStatusMessage("Mock: Unpaired " + device->name);
    }
#else
    removeDevice(address);
#endif
}

void BluetoothManager::trustDevice(const QString &address, bool trusted)
{
#ifdef Q_OS_WIN
    BluetoothDevice *device = m_deviceModel->findDevice(address);
    if (device) {
        device->trusted = trusted;
        m_deviceModel->updateDevice(address, *device);
        setStatusMessage(QString("Mock: Device %1 trusted").arg(trusted ? "" : "un"));
    }
#else
    if (!m_adapterInterface) {
        return;
    }

    QString devicePath = addressToPath(address);

    QDBusInterface deviceInterface(
        "org.bluez",
        devicePath,
        "org.bluez.Device1",
        QDBusConnection::systemBus()
    );

    if (deviceInterface.isValid()) {
        deviceInterface.setProperty("Trusted", trusted);
        qDebug() << "BluetoothManager: Device" << address << "trusted:" << trusted;
    }
#endif
}

void BluetoothManager::removeDevice(const QString &address)
{
#ifdef Q_OS_WIN
    m_deviceModel->removeDevice(address);
    emit deviceCountChanged();
    setStatusMessage("Mock: Device removed");
#else
    if (!m_adapterInterface) {
        return;
    }

    qDebug() << "BluetoothManager: Removing device:" << address;

    QString devicePath = addressToPath(address);

    QDBusMessage removeMsg = QDBusMessage::createMethodCall(
        "org.bluez",
        m_adapterPath,
        "org.bluez.Adapter1",
        "RemoveDevice"
    );
    removeMsg << QVariant::fromValue(QDBusObjectPath(devicePath));

    auto pending = QDBusConnection::systemBus().asyncCall(removeMsg);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, address](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            QString errorMsg = "Remove failed: " + reply.error().message();
            qWarning() << "BluetoothManager:" << errorMsg;
            emit error(errorMsg);
        } else {
            qDebug() << "BluetoothManager: Device removed";
            m_deviceModel->removeDevice(address);
            emit deviceCountChanged();
            emit deviceUnpaired(address);
            setStatusMessage("Device removed");
        }
        w->deleteLater();
    });
#endif
}

// ========================================================================
// ADAPTER MANAGEMENT
// ========================================================================

void BluetoothManager::setAdapterPower(bool powered)
{
#ifdef Q_OS_WIN
    m_adapterPowered = powered;
    emit adapterPoweredChanged();
    setStatusMessage(QString("Mock: Bluetooth %1").arg(powered ? "ON" : "OFF"));
#else
    if (!m_adapterInterface) {
        return;
    }

    m_adapterInterface->setProperty("Powered", powered);
    qDebug() << "BluetoothManager: Adapter power set to:" << powered;
#endif
}

void BluetoothManager::refreshDeviceList()
{
#ifdef Q_OS_WIN
    setStatusMessage("Mock: Refreshing device list");
#else
    if (!m_adapterInterface) {
        return;
    }

    qDebug() << "BluetoothManager: Refreshing device list...";
    m_deviceModel->clear();
    loadExistingDevices();
    setStatusMessage("Device list refreshed");
#endif
}

QString BluetoothManager::getDeviceName(const QString &address)
{
    BluetoothDevice *device = m_deviceModel->findDevice(address);
    return device ? device->name : QString();
}

bool BluetoothManager::isDeviceConnected(const QString &address)
{
    BluetoothDevice *device = m_deviceModel->findDevice(address);
    return device ? device->connected : false;
}

bool BluetoothManager::isDevicePaired(const QString &address)
{
    BluetoothDevice *device = m_deviceModel->findDevice(address);
    return device ? device->paired : false;
}

QString BluetoothManager::getFirstConnectedDeviceAddress()
{
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::ConnectedRole).toBool()) {
            return m_deviceModel->data(index, BluetoothDeviceModel::AddressRole).toString();
        }
    }
    return QString();
}

QString BluetoothManager::getFirstPairedDeviceAddress()
{
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::PairedRole).toBool() &&
            m_deviceModel->data(index, BluetoothDeviceModel::TrustedRole).toBool()) {
            QString address = m_deviceModel->data(index, BluetoothDeviceModel::AddressRole).toString();
            qDebug() << "BluetoothManager: Found paired device:" << address;
            return address;
        }
    }
    return QString();
}

int BluetoothManager::getConnectedDeviceBattery()
{
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::ConnectedRole).toBool()) {
            return m_deviceModel->data(index, BluetoothDeviceModel::BatteryLevelRole).toInt();
        }
    }
    return -1;
}

bool BluetoothManager::isConnectedDeviceCharging()
{
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::ConnectedRole).toBool()) {
            return m_deviceModel->data(index, BluetoothDeviceModel::IsChargingRole).toBool();
        }
    }
    return false;
}

int BluetoothManager::getConnectedDeviceSignal()
{
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::ConnectedRole).toBool()) {
            int rssi = m_deviceModel->data(index, BluetoothDeviceModel::SignalStrengthRole).toInt();
            if (rssi >= -50) return 4;
            if (rssi >= -60) return 3;
            if (rssi >= -70) return 2;
            if (rssi >= -80) return 1;
            return 0;
        }
    }
    return 0;
}

// ========================================================================
// DBUS SIGNAL HANDLERS
// ========================================================================

#ifndef Q_OS_WIN

void BluetoothManager::onInterfacesAdded(const QDBusObjectPath &path, const QVariantMap &interfaces)
{
    QString pathStr = path.path();

    if (!pathStr.startsWith(m_adapterPath + "/dev_")) {
        return;
    }

    if (!interfaces.contains("org.bluez.Device1")) {
        return;
    }

    qDebug() << "\n--- New device discovered during scan:" << pathStr;

    QVariantMap deviceProps = getDeviceProperties(pathStr);

    if (deviceProps.isEmpty()) {
        qWarning() << "BluetoothManager: Failed to get properties for discovered device" << pathStr;
        return;
    }

    BluetoothDevice device = parseDeviceProperties(pathStr, deviceProps);

    qDebug() << "BluetoothManager: New device found:" << device.name << "(" << device.address << ")";

    m_deviceModel->addDevice(device);
    emit deviceCountChanged();
    emit deviceFound(device.address, device.name);

    // Monitor this device's properties
    QDBusConnection::systemBus().connect(
        "org.bluez",
        pathStr,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList))
    );
}

void BluetoothManager::onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces)
{
    QString pathStr = path.path();

    if (!pathStr.startsWith(m_adapterPath + "/dev_")) {
        return;
    }

    if (!interfaces.contains("org.bluez.Device1")) {
        return;
    }

    QString address = pathToAddress(pathStr);
    qDebug() << "BluetoothManager: Device removed:" << address;

    m_deviceModel->removeDevice(address);
    emit deviceCountChanged();
}

void BluetoothManager::onPropertiesChanged(const QString &interface,
                                          const QVariantMap &changedProperties,
                                          const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties);

    if (interface == "org.bluez.Adapter1") {
        if (changedProperties.contains("Powered")) {
            bool powered = changedProperties.value("Powered").toBool();
            if (m_adapterPowered != powered) {
                m_adapterPowered = powered;
                emit adapterPoweredChanged();
                qDebug() << "BluetoothManager: Adapter powered:" << powered;
            }
        }
    } else if (interface == "org.bluez.Device1") {
        for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
            QModelIndex index = m_deviceModel->index(i, 0);
            QString devicePath = m_deviceModel->data(index, BluetoothDeviceModel::PathRole).toString();

            QVariantMap props = getDeviceProperties(devicePath);
            if (!props.isEmpty()) {
                BluetoothDevice device = parseDeviceProperties(devicePath, props);
                QString address = device.address;

                BluetoothDevice *existingDevice = m_deviceModel->findDevice(address);
                if (existingDevice) {
                    bool wasConnected = existingDevice->connected;
                    bool wasPaired = existingDevice->paired;

                    m_deviceModel->updateDevice(address, device);

                    if (!wasConnected && device.connected) {
                        emit deviceConnected(address);
                        setStatusMessage("Connected to " + device.name);
                    } else if (wasConnected && !device.connected) {
                        emit deviceDisconnected(address);
                        setStatusMessage("Disconnected from " + device.name);
                    }

                    if (!wasPaired && device.paired) {
                        emit devicePaired(address);
                        setStatusMessage("Paired with " + device.name);
                    }
                }
            }
        }
    }
}

#endif // Q_OS_WIN

// ========================================================================
// MOCK DATA GENERATION
// ========================================================================

void BluetoothManager::generateMockDevices()
{
    qDebug() << "BluetoothManager: Generating mock devices...";

    QList<BluetoothDevice> mockDevices;

    BluetoothDevice phone;
    phone.name = "iPhone 15 Pro";
    phone.address = "A1:B2:C3:D4:E5:F6";
    phone.paired = true;
    phone.connected = false;
    phone.trusted = true;
    phone.signalStrength = -65;
    phone.icon = "phone";
    mockDevices.append(phone);

    BluetoothDevice tablet;
    tablet.name = "iPad Air";
    tablet.address = "11:22:33:44:55:66";
    tablet.paired = true;
    tablet.connected = false;
    tablet.trusted = true;
    tablet.signalStrength = -80;
    tablet.icon = "computer";
    mockDevices.append(tablet);

    BluetoothDevice speaker;
    speaker.name = "JBL Speaker";
    speaker.address = "AA:BB:CC:DD:EE:FF";
    speaker.paired = false;
    speaker.connected = false;
    speaker.trusted = false;
    speaker.signalStrength = -55;
    speaker.icon = "audio-card";
    mockDevices.append(speaker);

    for (const BluetoothDevice &device : mockDevices) {
        m_deviceModel->addDevice(device);
    }

    emit deviceCountChanged();
    setStatusMessage("Mock: 3 devices found");

    qDebug() << "BluetoothManager: Generated" << mockDevices.count() << "mock devices";
}
