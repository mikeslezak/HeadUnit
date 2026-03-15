#ifndef ANCSMANAGER_H
#define ANCSMANAGER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QTimer>
#include <QMap>
#include <QByteArray>
#include <QProcess>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusMessage>
#endif

/**
 * AncsManager — Apple Notification Center Service via BlueZ D-Bus
 *
 * Architecture:
 *   1. Registers as a BLE peripheral via LEAdvertisement1 (SolicitUUIDs = ANCS)
 *   2. iPhone sees the device in Bluetooth settings and initiates BLE connection
 *   3. After bonding, iPhone exposes ANCS GATT service
 *   4. This class discovers ANCS characteristics via D-Bus ObjectManager
 *   5. Subscribes to Notification Source + Data Source via GattCharacteristic1.StartNotify()
 *   6. Writes GetNotificationAttributes to Control Point for full notification details
 *
 * The iPhone must initiate the BLE connection — we cannot connect to it as a central.
 */
class AncsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool advertising READ isAdvertising NOTIFY advertisingChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)

public:
    explicit AncsManager(QObject *parent = nullptr);
    ~AncsManager();

    bool isAdvertising() const { return m_advertising; }
    bool isConnected() const { return m_connected; }

    /// Start BLE peripheral advertising with ANCS service solicitation
    Q_INVOKABLE void startAdvertising();

    /// Stop BLE advertising
    Q_INVOKABLE void stopAdvertising();

signals:
    void advertisingChanged();
    void connectedChanged();

    /// Emitted when a notification arrives with full details
    void notificationReceived(const QVariantMap &notification);

    /// Emitted when a notification is removed on the phone
    void notificationRemoved(const QString &notificationId);

    /// Emitted when LE pairing succeeds — triggers BR/EDR profile connections
    void deviceBondedOverLE(const QString &address);

private slots:
#ifndef Q_OS_WIN
    void onInterfacesAdded(const QDBusObjectPath &path, const QVariantMap &interfaces);
    void onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
    void onNotificationSourceChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
    void onDataSourceChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
#endif

private:
#ifndef Q_OS_WIN
    // BLE Advertisement via D-Bus
    bool registerAdvertisement();
    void unregisterAdvertisement();

    // GATT characteristic discovery and subscription
    void checkForAncsCharacteristics();
    void subscribeToCharacteristic(const QString &charPath);
    void handleNotificationSourceValue(const QByteArray &value);
    void handleDataSourceValue(const QByteArray &value);
    void requestNotificationAttributes(quint32 uid);

    // BR/EDR profile connection after LE bond
    void connectClassicProfiles(const QString &devicePath);
    QString extractDeviceAddress(const QString &charPath);

    // Notification parsing
    QString resolveAppName(const QString &bundleId);

    // ANCS UUIDs (lowercase for D-Bus comparison)
    static const QString ANCS_SERVICE_UUID;
    static const QString NOTIFICATION_SOURCE_UUID;
    static const QString CONTROL_POINT_UUID;
    static const QString DATA_SOURCE_UUID;

    // State
    bool m_advertising;
    bool m_connected;
    bool m_advertisementRegistered;

    // D-Bus paths to discovered ANCS characteristics
    QString m_notificationSourcePath;
    QString m_controlPointPath;
    QString m_dataSourcePath;

    // Device tracking
    QString m_bondedDevicePath;  // e.g., /org/bluez/hci0/dev_80_96_98_C8_69_17
    bool m_classicConnected;

    // Data Source response reassembly
    QByteArray m_dataBuffer;
    QMap<quint32, QVariantMap> m_pendingNotifications;

    // Monitor timer — periodically check for ANCS characteristics after device connects
    QTimer *m_discoveryTimer;

    // Python advertisement helper process
    QProcess *m_advProcess = nullptr;
#endif
};

#endif // ANCSMANAGER_H
