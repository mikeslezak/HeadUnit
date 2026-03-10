#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QProcess>
#include <QDateTime>
#include "BluetoothDeviceModel.h"
#include "TelephonyManager.h"

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#endif

class ContactManager;

/**
 * BluetoothManager - Manages Bluetooth device discovery, pairing, and connections
 *
 * Integrates with BlueZ via DBus for device management.
 * Telephony (oFono calls) is delegated to TelephonyManager.
 */
class BluetoothManager : public QObject
{
    Q_OBJECT

    // BT core properties
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(bool adapterPowered READ adapterPowered NOTIFY adapterPoweredChanged)
    Q_PROPERTY(bool adapterAvailable READ adapterAvailable NOTIFY adapterAvailableChanged)
    Q_PROPERTY(BluetoothDeviceModel* deviceModel READ deviceModel CONSTANT)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    // Telephony properties (forwarded from TelephonyManager for QML compatibility)
    Q_PROPERTY(int cellularSignal READ cellularSignal NOTIFY cellularSignalChanged)
    Q_PROPERTY(QString carrierName READ carrierName NOTIFY carrierNameChanged)
    Q_PROPERTY(bool hasActiveCall READ hasActiveCall NOTIFY activeCallChanged)
    Q_PROPERTY(QString activeCallName READ activeCallName NOTIFY activeCallChanged)
    Q_PROPERTY(QString activeCallNumber READ activeCallNumber NOTIFY activeCallChanged)
    Q_PROPERTY(QString activeCallState READ activeCallState NOTIFY activeCallChanged)
    Q_PROPERTY(int activeCallDuration READ activeCallDuration NOTIFY activeCallChanged)
    Q_PROPERTY(bool isCallMuted READ isCallMuted NOTIFY callMutedChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);
    ~BluetoothManager();

    // BT core getters
    bool isScanning() const { return m_isScanning; }
    bool adapterPowered() const { return m_adapterPowered; }
    bool adapterAvailable() const { return m_adapterAvailable; }
    BluetoothDeviceModel* deviceModel() { return m_deviceModel; }
    int deviceCount() const { return m_deviceModel->rowCount(); }
    QString statusMessage() const { return m_statusMessage; }

    // Telephony forwarding getters
    int cellularSignal() const { return m_telephonyManager->cellularSignal(); }
    QString carrierName() const { return m_telephonyManager->carrierName(); }
    bool hasActiveCall() const { return m_telephonyManager->hasActiveCall(); }
    QString activeCallName() const { return m_telephonyManager->activeCallName(); }
    QString activeCallNumber() const { return m_telephonyManager->activeCallNumber(); }
    QString activeCallState() const { return m_telephonyManager->activeCallState(); }
    int activeCallDuration() const { return m_telephonyManager->activeCallDuration(); }
    bool isCallMuted() const { return m_telephonyManager->isCallMuted(); }

    // Access to sub-managers
    TelephonyManager* telephonyManager() { return m_telephonyManager; }

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
    void setContactManager(ContactManager* contactManager);

    // Device queries
    Q_INVOKABLE QString getDeviceName(const QString &address);
    Q_INVOKABLE bool isDeviceConnected(const QString &address);
    Q_INVOKABLE bool isDevicePaired(const QString &address);
    Q_INVOKABLE QString getFirstConnectedDeviceAddress();
    Q_INVOKABLE QString getFirstPairedDeviceAddress();
    Q_INVOKABLE int getConnectedDeviceBattery();
    Q_INVOKABLE bool isConnectedDeviceCharging();
    Q_INVOKABLE int getConnectedDeviceSignal();

    // Telephony forwarding (for QML compatibility)
    Q_INVOKABLE void dialNumber(const QString &phoneNumber) { m_telephonyManager->dialNumber(phoneNumber); }
    Q_INVOKABLE void answerCall() { m_telephonyManager->answerCall(); }
    Q_INVOKABLE void hangupCall() { m_telephonyManager->hangupCall(); }
    Q_INVOKABLE void sendDTMF(const QString &tones) { m_telephonyManager->sendDTMF(tones); }
    Q_INVOKABLE void toggleMute() { m_telephonyManager->toggleMute(); }

signals:
    // BT core signals
    void scanningChanged();
    void adapterPoweredChanged();
    void adapterAvailableChanged();
    void deviceCountChanged();
    void statusMessageChanged();

    void deviceFound(const QString &address, const QString &name);
    void deviceConnected(const QString &address);
    void deviceDisconnected(const QString &address);
    void devicePaired(const QString &address);
    void deviceUnpaired(const QString &address);
    void error(const QString &message);

    // Telephony signals (forwarded)
    void activeCallChanged();
    void callMutedChanged();
    void cellularSignalChanged();
    void carrierNameChanged();

private slots:
    void onScanTimeout();

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

    BluetoothDeviceModel *m_deviceModel;
    TelephonyManager *m_telephonyManager;
    QTimer *m_scanTimer;

#ifndef Q_OS_WIN
    QDBusInterface *m_adapterInterface;
#endif

    ContactManager* m_contactManager;
    bool m_mockMode;
};

#endif // BLUETOOTHMANAGER_H
