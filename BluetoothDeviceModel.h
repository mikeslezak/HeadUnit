#ifndef BLUETOOTHDEVICEMODEL_H
#define BLUETOOTHDEVICEMODEL_H

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QList>

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

#endif // BLUETOOTHDEVICEMODEL_H
