#include "BluetoothManager.h"
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
#endif

// ========================================================================
// BLUETOOTH DEVICE MODEL IMPLEMENTATION
// ========================================================================

BluetoothDeviceModel::BluetoothDeviceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int BluetoothDeviceModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_devices.count();
}

QVariant BluetoothDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_devices.count()) {
        return QVariant();
    }

    const BluetoothDevice &device = m_devices.at(index.row());

    switch (role) {
        case NameRole:
            return device.name;
        case AddressRole:
            return device.address;
        case PairedRole:
            return device.paired;
        case ConnectedRole:
            return device.connected;
        case TrustedRole:
            return device.trusted;
        case SignalStrengthRole:
            return device.signalStrength;
        case BatteryLevelRole:
            return device.batteryLevel;
        case IsChargingRole:
            return device.isCharging;
        case IconRole:
            return device.icon;
        case PathRole:
            return device.path;
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> BluetoothDeviceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "deviceName";
    roles[AddressRole] = "deviceAddress";
    roles[PairedRole] = "isPaired";
    roles[ConnectedRole] = "isConnected";
    roles[TrustedRole] = "isTrusted";
    roles[SignalStrengthRole] = "signalStrength";
    roles[BatteryLevelRole] = "batteryLevel";
    roles[IsChargingRole] = "isCharging";
    roles[IconRole] = "deviceIcon";
    roles[PathRole] = "devicePath";
    return roles;
}

void BluetoothDeviceModel::addDevice(const BluetoothDevice &device)
{
    // Check if device already exists
    int existingIndex = findDeviceIndex(device.address);
    if (existingIndex >= 0) {
        updateDevice(device.address, device);
        return;
    }

    beginInsertRows(QModelIndex(), m_devices.count(), m_devices.count());
    m_devices.append(device);
    endInsertRows();
}

void BluetoothDeviceModel::updateDevice(const QString &address, const BluetoothDevice &device)
{
    int index = findDeviceIndex(address);
    if (index < 0) {
        addDevice(device);
        return;
    }

    m_devices[index] = device;
    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex);
}

void BluetoothDeviceModel::removeDevice(const QString &address)
{
    int index = findDeviceIndex(address);
    if (index < 0) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_devices.removeAt(index);
    endRemoveRows();
}

void BluetoothDeviceModel::clear()
{
    beginResetModel();
    m_devices.clear();
    endResetModel();
}

BluetoothDevice* BluetoothDeviceModel::findDevice(const QString &address)
{
    for (int i = 0; i < m_devices.count(); ++i) {
        if (m_devices[i].address == address) {
            return &m_devices[i];
        }
    }
    return nullptr;
}

int BluetoothDeviceModel::findDeviceIndex(const QString &address)
{
    for (int i = 0; i < m_devices.count(); ++i) {
        if (m_devices[i].address == address) {
            return i;
        }
    }
    return -1;
}

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
    , m_cellularSignal(0)
    , m_carrierName("")
    , m_deviceModel(new BluetoothDeviceModel(this))
    , m_scanTimer(new QTimer(this))
    , m_ofonoUpdateTimer(new QTimer(this))
#ifndef Q_OS_WIN
    , m_adapterInterface(nullptr)
#endif
{
#ifdef Q_OS_WIN
    m_mockMode = true;
    qDebug() << "BluetoothManager: Running in MOCK mode (Windows)";
    setStatusMessage("Mock Mode - Simulated Bluetooth");
    m_adapterAvailable = true;
    m_adapterPowered = true;
    emit adapterAvailableChanged();
    emit adapterPoweredChanged();

    // Generate mock devices
    QTimer::singleShot(500, this, &BluetoothManager::generateMockDevices);
#else
    m_mockMode = false;
    qDebug() << "BluetoothManager: Real BlueZ mode";
    initialize();
#endif

    // Setup scan timeout timer
    connect(m_scanTimer, &QTimer::timeout, this, &BluetoothManager::onScanTimeout);
    m_scanTimer->setInterval(30000); // 30 second scan timeout

    // Setup oFono signal update timer (update every 10 seconds)
    connect(m_ofonoUpdateTimer, &QTimer::timeout, this, &BluetoothManager::updateOfonoSignal);
    m_ofonoUpdateTimer->setInterval(10000); // 10 seconds
    m_ofonoUpdateTimer->start();
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

// ========================================================================
// INITIALIZATION
// ========================================================================

void BluetoothManager::initialize()
{
#ifndef Q_OS_WIN
    qDebug() << "BluetoothManager: Initializing BlueZ adapter...";

    // Create adapter interface
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

    // Get adapter powered state
    QVariant poweredVar = m_adapterInterface->property("Powered");
    m_adapterPowered = poweredVar.toBool();
    emit adapterPoweredChanged();

    qDebug() << "BluetoothManager: Adapter available, powered:" << m_adapterPowered;

    // Setup DBus monitoring
    setupDBusMonitoring();

    // Load existing devices
    loadExistingDevices();

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

    // Use ObjectManager to get all devices
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

        // Check if this is a device under our adapter
        if (objectPath.startsWith(m_adapterPath + "/dev_") &&
            interfaces.contains("org.bluez.Device1")) {

            qDebug() << "\n--- Found device at path:" << objectPath;

            // Get fresh properties using GetAll - this is more reliable than cached data
            QVariantMap deviceProps = getDeviceProperties(objectPath);

            if (deviceProps.isEmpty()) {
                qWarning() << "BluetoothManager: Failed to get properties for" << objectPath;
                continue;
            }

            BluetoothDevice device = parseDeviceProperties(objectPath, deviceProps);

            // Try to get battery level from org.bluez.Battery1 interface if available
            if (interfaces.contains("org.bluez.Battery1")) {
                QVariantMap batteryProps = getBatteryProperties(objectPath);
                if (!batteryProps.isEmpty()) {
                    int newBattery = batteryProps.value("Percentage", -1).toInt();
                    device.batteryLevel = newBattery;

                    // Detect charging by tracking battery history
                    if (newBattery >= 0) {
                        m_batteryHistory.append(newBattery);
                        if (m_batteryHistory.size() > 3) {
                            m_batteryHistory.removeFirst();  // Keep only last 3 readings
                        }

                        // If we have at least 2 readings and battery is increasing, it's charging
                        if (m_batteryHistory.size() >= 2) {
                            device.isCharging = m_batteryHistory.last() > m_batteryHistory.first();
                        }
                    }

                    qDebug() << "  Found Battery1 interface - Battery:" << device.batteryLevel << "% Charging:" << device.isCharging;
                }
            }

            // Load ALL devices (paired, connected, or previously seen)
            // This ensures connected devices appear immediately
            m_deviceModel->addDevice(device);
            deviceCount++;

            if (device.connected) {
                connectedCount++;
                qDebug() << "BluetoothManager: ✓ Loaded CONNECTED device:" << device.name << "(" << device.address << ")";
            } else if (device.paired) {
                qDebug() << "BluetoothManager: ✓ Loaded paired device:" << device.name << "(" << device.address << ")";
            } else {
                qDebug() << "BluetoothManager: ✓ Loaded discovered device:" << device.name << "(" << device.address << ")";
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

    // Log ALL property values for debugging
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        qDebug() << "      [" << it.key() << "]" << "=" << it.value() << "(" << it.value().typeName() << ")";
    }

    // Try to get device name - Alias is user-friendly name, Name is device name
    QString alias = properties.value("Alias").toString();
    QString name = properties.value("Name").toString();
    QString address = properties.value("Address").toString();

    qDebug() << "      Extracted values:";
    qDebug() << "        - Alias:" << (alias.isEmpty() ? "(empty)" : alias);
    qDebug() << "        - Name:" << (name.isEmpty() ? "(empty)" : name);
    qDebug() << "        - Address:" << (address.isEmpty() ? "(empty)" : address);

    // Prefer Alias (user-set name), fallback to Name (device name), fallback to address
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
    device.batteryLevel = properties.value("Battery", -1).toInt();  // May not be available
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
    // Extract dev_XX_XX_XX_XX_XX_XX from path
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

    // Simulate finding a new device after 2 seconds
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

    // Start discovery
    QDBusMessage response = m_adapterInterface->call("StartDiscovery");

    if (response.type() == QDBusMessage::ErrorMessage) {
        QString errorMsg = "Failed to start scan: " + response.errorMessage();
        qWarning() << "BluetoothManager:" << errorMsg;
        setStatusMessage("Scan failed");
        emit error(errorMsg);
        return;
    }

    m_isScanning = true;
    emit scanningChanged();
    setStatusMessage("Scanning for devices...");

    // Start timeout timer
    m_scanTimer->start();

    qDebug() << "BluetoothManager: Scan started";
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

    QDBusMessage response = m_adapterInterface->call("StopDiscovery");

    if (response.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "BluetoothManager: StopDiscovery failed:" << response.errorMessage();
    }

    m_isScanning = false;
    m_scanTimer->stop();
    emit scanningChanged();
    setStatusMessage("Scan stopped");

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
    setStatusMessage("Connecting to device...");

    QString devicePath = addressToPath(address);

    QDBusInterface deviceInterface(
        "org.bluez",
        devicePath,
        "org.bluez.Device1",
        QDBusConnection::systemBus()
    );

    if (!deviceInterface.isValid()) {
        QString errorMsg = "Device interface not valid: " + deviceInterface.lastError().message();
        qWarning() << "BluetoothManager:" << errorMsg;
        emit error(errorMsg);
        setStatusMessage("Connection failed");
        return;
    }

    // Connect asynchronously
    QDBusMessage response = deviceInterface.call("Connect");

    if (response.type() == QDBusMessage::ErrorMessage) {
        QString errorMsg = "Connection failed: " + response.errorMessage();
        qWarning() << "BluetoothManager:" << errorMsg;
        emit error(errorMsg);
        setStatusMessage("Connection failed");
    } else {
        qDebug() << "BluetoothManager: Connection initiated";
        setStatusMessage("Connecting...");
        // The connection status will be updated via PropertiesChanged signal
    }
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

    QDBusInterface deviceInterface(
        "org.bluez",
        devicePath,
        "org.bluez.Device1",
        QDBusConnection::systemBus()
    );

    if (!deviceInterface.isValid()) {
        qWarning() << "BluetoothManager: Device interface not valid";
        return;
    }

    QDBusMessage response = deviceInterface.call("Disconnect");

    if (response.type() == QDBusMessage::ErrorMessage) {
        QString errorMsg = "Disconnect failed: " + response.errorMessage();
        qWarning() << "BluetoothManager:" << errorMsg;
        emit error(errorMsg);
    } else {
        qDebug() << "BluetoothManager: Disconnect initiated";
        setStatusMessage("Disconnecting...");
    }
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
    setStatusMessage("Pairing device...");

    QString devicePath = addressToPath(address);

    QDBusInterface deviceInterface(
        "org.bluez",
        devicePath,
        "org.bluez.Device1",
        QDBusConnection::systemBus()
    );

    if (!deviceInterface.isValid()) {
        QString errorMsg = "Device interface not valid";
        qWarning() << "BluetoothManager:" << errorMsg;
        emit error(errorMsg);
        return;
    }

    // Pair asynchronously
    QDBusMessage response = deviceInterface.call("Pair");

    if (response.type() == QDBusMessage::ErrorMessage) {
        QString errorMsg = "Pairing failed: " + response.errorMessage();
        qWarning() << "BluetoothManager:" << errorMsg;
        emit error(errorMsg);
        setStatusMessage("Pairing failed");
    } else {
        qDebug() << "BluetoothManager: Pairing initiated";
        setStatusMessage("Pairing...");
    }
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

    QDBusMessage response = QDBusConnection::systemBus().call(removeMsg);

    if (response.type() == QDBusMessage::ErrorMessage) {
        QString errorMsg = "Remove failed: " + response.errorMessage();
        qWarning() << "BluetoothManager:" << errorMsg;
        emit error(errorMsg);
    } else {
        qDebug() << "BluetoothManager: Device removed";
        m_deviceModel->removeDevice(address);
        emit deviceCountChanged();
        emit deviceUnpaired(address);
        setStatusMessage("Device removed");
    }
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

int BluetoothManager::getConnectedDeviceBattery()
{
    // Find the first connected device
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::ConnectedRole).toBool()) {
            return m_deviceModel->data(index, BluetoothDeviceModel::BatteryLevelRole).toInt();
        }
    }
    return -1;  // No connected device
}

bool BluetoothManager::isConnectedDeviceCharging()
{
    // Find the first connected device
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::ConnectedRole).toBool()) {
            return m_deviceModel->data(index, BluetoothDeviceModel::IsChargingRole).toBool();
        }
    }
    return false;  // No connected device
}

int BluetoothManager::getConnectedDeviceSignal()
{
    // Find the first connected device and convert RSSI to signal bars (0-4)
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::ConnectedRole).toBool()) {
            int rssi = m_deviceModel->data(index, BluetoothDeviceModel::SignalStrengthRole).toInt();
            // Convert RSSI to signal bars: -100 to -50 dBm range
            // Excellent: > -50 dBm (4 bars)
            // Good: -50 to -60 dBm (3 bars)
            // Fair: -60 to -70 dBm (2 bars)
            // Weak: -70 to -80 dBm (1 bar)
            // Very weak: < -80 dBm (0 bars)
            if (rssi >= -50) return 4;
            if (rssi >= -60) return 3;
            if (rssi >= -70) return 2;
            if (rssi >= -80) return 1;
            return 0;
        }
    }
    return 0;  // No connected device
}

// ========================================================================
// DBUS SIGNAL HANDLERS
// ========================================================================

#ifndef Q_OS_WIN

void BluetoothManager::onInterfacesAdded(const QDBusObjectPath &path, const QVariantMap &interfaces)
{
    QString pathStr = path.path();

    // Check if this is a device under our adapter
    if (!pathStr.startsWith(m_adapterPath + "/dev_")) {
        return;
    }

    // Check if it has Device1 interface
    if (!interfaces.contains("org.bluez.Device1")) {
        return;
    }

    qDebug() << "\n--- New device discovered during scan:" << pathStr;

    // Get fresh properties using GetAll - more reliable than signal data
    QVariantMap deviceProps = getDeviceProperties(pathStr);

    if (deviceProps.isEmpty()) {
        qWarning() << "BluetoothManager: Failed to get properties for discovered device" << pathStr;
        return;
    }

    BluetoothDevice device = parseDeviceProperties(pathStr, deviceProps);

    qDebug() << "BluetoothManager: ✓ New device found:" << device.name << "(" << device.address << ")";

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
        // Adapter property changed
        if (changedProperties.contains("Powered")) {
            bool powered = changedProperties.value("Powered").toBool();
            if (m_adapterPowered != powered) {
                m_adapterPowered = powered;
                emit adapterPoweredChanged();
                qDebug() << "BluetoothManager: Adapter powered:" << powered;
            }
        }
    } else if (interface == "org.bluez.Device1") {
        // Device property changed
        QObject *senderObj = sender();
        if (!senderObj) {
            return;
        }

        // Extract device path from sender's DBus connection
        // This is a bit tricky - we need to find which device this belongs to
        // For now, we'll refresh all devices if any property changes
        // A more efficient approach would be to track device paths per signal connection

        // Common approach: Update all devices when properties change
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

                    // Emit signals for state changes
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
// OFONO SIGNAL READING
// ========================================================================

void BluetoothManager::updateOfonoSignal()
{
#ifndef Q_OS_WIN
    // Try to get connected device's oFono modem path
    QString connectedAddress;
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        QModelIndex index = m_deviceModel->index(i, 0);
        if (m_deviceModel->data(index, BluetoothDeviceModel::ConnectedRole).toBool()) {
            connectedAddress = m_deviceModel->data(index, BluetoothDeviceModel::AddressRole).toString();
            break;
        }
    }

    if (connectedAddress.isEmpty()) {
        // No connected device, reset signal
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
        // oFono modem not available or not powered
        return;
    }

    // Parse the reply
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

    // Extract signal strength (0-100) and convert to bars (0-4)
    int strengthPercent = properties.value("Strength", 0).toInt();
    int signalBars = 0;
    if (strengthPercent >= 80) signalBars = 4;
    else if (strengthPercent >= 60) signalBars = 3;
    else if (strengthPercent >= 40) signalBars = 2;
    else if (strengthPercent >= 20) signalBars = 1;

    QString carrier = properties.value("Name", "").toString();

    // Update and emit signals if changed
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
        qDebug() << "BluetoothManager: Cellular signal updated -"
                 << "Bars:" << m_cellularSignal
                 << "(" << strengthPercent << "%)"
                 << "Carrier:" << m_carrierName;
    }
#endif
}

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
