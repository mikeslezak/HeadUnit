#ifndef TELEPHONYMANAGER_H
#define TELEPHONYMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QDateTime>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusVariant>
#endif

class ContactManager;
class BluetoothDeviceModel;

/**
 * TelephonyManager - Manages phone calls via oFono over Bluetooth HFP
 *
 * Handles:
 * - Outgoing/incoming call lifecycle (dial, answer, hangup)
 * - Call state monitoring via oFono DBus signals
 * - DTMF tone sending
 * - Mute toggle
 * - Cellular signal strength and carrier name via oFono
 */
class TelephonyManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasActiveCall READ hasActiveCall NOTIFY activeCallChanged)
    Q_PROPERTY(QString activeCallName READ activeCallName NOTIFY activeCallChanged)
    Q_PROPERTY(QString activeCallNumber READ activeCallNumber NOTIFY activeCallChanged)
    Q_PROPERTY(QString activeCallState READ activeCallState NOTIFY activeCallChanged)
    Q_PROPERTY(int activeCallDuration READ activeCallDuration NOTIFY activeCallChanged)
    Q_PROPERTY(bool isCallMuted READ isCallMuted NOTIFY callMutedChanged)
    Q_PROPERTY(int cellularSignal READ cellularSignal NOTIFY cellularSignalChanged)
    Q_PROPERTY(QString carrierName READ carrierName NOTIFY carrierNameChanged)

public:
    explicit TelephonyManager(QObject *parent = nullptr);

    // Property getters
    bool hasActiveCall() const { return m_hasActiveCall; }
    QString activeCallName() const { return m_activeCallName; }
    QString activeCallNumber() const { return m_activeCallNumber; }
    QString activeCallState() const { return m_activeCallState; }
    int activeCallDuration() const { return m_activeCallDuration; }
    bool isCallMuted() const { return m_isCallMuted; }
    int cellularSignal() const { return m_cellularSignal; }
    QString carrierName() const { return m_carrierName; }

    // Dependencies
    void setContactManager(ContactManager* contactManager);
    void setDeviceModel(BluetoothDeviceModel* model);
    void setAdapterPath(const QString& path);

    // Set up oFono DBus monitoring (called after BT adapter is ready)
    void setupOfonoMonitoring();

public slots:
    Q_INVOKABLE void dialNumber(const QString &phoneNumber);
    Q_INVOKABLE void answerCall();
    Q_INVOKABLE void hangupCall();
    Q_INVOKABLE void sendDTMF(const QString &tones);
    Q_INVOKABLE void toggleMute();

signals:
    void activeCallChanged();
    void callMutedChanged();
    void cellularSignalChanged();
    void carrierNameChanged();

private slots:
    void updateOfonoSignal();
    void updateCallDuration();

#ifndef Q_OS_WIN
    void onCallPropertyChanged(const QString &propertyName, const QDBusVariant &value);
    void onCallAdded(const QDBusObjectPath &path, const QVariantMap &properties);
    void onCallRemoved(const QDBusObjectPath &path);
#endif

private:
    // Call state
    bool m_hasActiveCall;
    QString m_activeCallName;
    QString m_activeCallNumber;
    QString m_activeCallState;
    QString m_activeCallPath;
    int m_activeCallDuration;
    QDateTime m_callStartTime;
    bool m_isCallMuted;

    // Cellular info
    int m_cellularSignal;
    QString m_carrierName;

    // Timers
    QTimer *m_ofonoUpdateTimer;
    QTimer *m_callDurationTimer;

    // Dependencies
    ContactManager* m_contactManager;
    BluetoothDeviceModel* m_deviceModel;
    QString m_adapterPath;
    bool m_mockMode;
};

#endif // TELEPHONYMANAGER_H
