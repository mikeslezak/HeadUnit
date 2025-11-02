#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QList>
#include <QTimer>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#endif

/**
 * BluetoothDevice - Represents a Bluetooth device
 */
struct BluetoothDevice {
    QString name;
    QString address;
    QString path;  // DBus object path
    bool paired;
    bool connected;
    bool trusted;
    int signalStrength;  // RSSI value (-100 to 0)
    int batteryLevel;     // Battery percentage (0-100, -1 if unavailable)
    bool isCharging;      // Detected by monitoring battery trend
    QString icon;  // Device icon type (phone, audio-card, etc.)

    BluetoothDevice()
        : paired(false), connected(false), trusted(false), signalStrength(-100), batteryLevel(-1), isCharging(false) {}
};

/**
 * BluetoothDeviceModel - QML-accessible model for device list
 */
class BluetoothDeviceModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        AddressRole,
        PairedRole,
        ConnectedRole,
        TrustedRole,
        SignalStrengthRole,
        BatteryLevelRole,
        IsChargingRole,
        IconRole,
        PathRole
    };

    explicit BluetoothDeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addDevice(const BluetoothDevice &device);
    void updateDevice(const QString &address, const BluetoothDevice &device);
    void removeDevice(const QString &address);
    void clear();

    BluetoothDevice* findDevice(const QString &address);
    int findDeviceIndex(const QString &address);

private:
    QList<BluetoothDevice> m_devices;
};

/**
 * BluetoothManager - Manages Bluetooth device discovery, pairing, and connections
 *
 * Integrates with BlueZ via DBus to:
 * - Scan for available devices
 * - Pair/unpair devices
 * - Connect/disconnect devices
 * - Monitor device status changes
 * - Manage trusted devices
 */
class BluetoothManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isScanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(bool adapterPowered READ adapterPowered NOTIFY adapterPoweredChanged)
    Q_PROPERTY(bool adapterAvailable READ adapterAvailable NOTIFY adapterAvailableChanged)
    Q_PROPERTY(BluetoothDeviceModel* deviceModel READ deviceModel CONSTANT)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int cellularSignal READ cellularSignal NOTIFY cellularSignalChanged)
    Q_PROPERTY(QString carrierName READ carrierName NOTIFY carrierNameChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);
    ~BluetoothManager();

    // Property getters
    bool isScanning() const { return m_isScanning; }
    bool adapterPowered() const { return m_adapterPowered; }
    bool adapterAvailable() const { return m_adapterAvailable; }
    BluetoothDeviceModel* deviceModel() { return m_deviceModel; }
    int deviceCount() const { return m_deviceModel->rowCount(); }
    QString statusMessage() const { return m_statusMessage; }
    int cellularSignal() const { return m_cellularSignal; }
    QString carrierName() const { return m_carrierName; }

public slots:
    // Scanning
    void startScan();
    void stopScan();

    // Device management
    void connectToDevice(const QString &address);
    void disconnectDevice(const QString &address);
    void pairDevice(const QString &address);
    void unpairDevice(const QString &address);
    void trustDevice(const QString &address, bool trusted);
    void removeDevice(const QString &address);

    // Adapter management
    void setAdapterPower(bool powered);
    void refreshDeviceList();

    // Get device info
    Q_INVOKABLE QString getDeviceName(const QString &address);
    Q_INVOKABLE bool isDeviceConnected(const QString &address);
    Q_INVOKABLE bool isDevicePaired(const QString &address);
    Q_INVOKABLE int getConnectedDeviceBattery();
    Q_INVOKABLE bool isConnectedDeviceCharging();
    Q_INVOKABLE int getConnectedDeviceSignal();

signals:
    void scanningChanged();
    void adapterPoweredChanged();
    void adapterAvailableChanged();
    void deviceCountChanged();
    void statusMessageChanged();
    void cellularSignalChanged();
    void carrierNameChanged();

    void deviceFound(const QString &address, const QString &name);
    void deviceConnected(const QString &address);
    void deviceDisconnected(const QString &address);
    void devicePaired(const QString &address);
    void deviceUnpaired(const QString &address);
    void error(const QString &message);

private slots:
    void onScanTimeout();
    void updateOfonoSignal();

#ifndef Q_OS_WIN
    void onInterfacesAdded(const QDBusObjectPath &path, const QVariantMap &interfaces);
    void onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
    void onPropertiesChanged(const QString &interface,
                            const QVariantMap &changedProperties,
                            const QStringList &invalidatedProperties);
#endif

private:
    void setStatusMessage(const QString &msg);
    void initialize();

#ifndef Q_OS_WIN
    void setupDBusMonitoring();
    void loadExistingDevices();
    BluetoothDevice parseDeviceProperties(const QString &path, const QVariantMap &properties);
    QString addressToPath(const QString &address);
    QString pathToAddress(const QString &path);
    QVariantMap getDeviceProperties(const QString &path);
    QVariantMap getBatteryProperties(const QString &path);
#endif

    void generateMockDevices();

    // Member variables
    bool m_isScanning;
    bool m_adapterPowered;
    bool m_adapterAvailable;
    QString m_statusMessage;
    QString m_adapterPath;
    int m_cellularSignal;
    QString m_carrierName;
    QList<int> m_batteryHistory;  // Track last few battery readings to detect charging

    BluetoothDeviceModel *m_deviceModel;
    QTimer *m_scanTimer;
    QTimer *m_ofonoUpdateTimer;

#ifndef Q_OS_WIN
    QDBusInterface *m_adapterInterface;
#endif

    bool m_mockMode;
};

#endif // BLUETOOTHMANAGER_H
