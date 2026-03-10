#include "BluetoothDeviceModel.h"

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
    roles[NameRole] = "name";
    roles[AddressRole] = "address";
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
        // Update existing device
        m_devices[existingIndex] = device;
        QModelIndex modelIndex = createIndex(existingIndex, 0);
        emit dataChanged(modelIndex, modelIndex);
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
