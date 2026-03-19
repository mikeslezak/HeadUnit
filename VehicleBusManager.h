#pragma once

#include <QObject>
#include <QTimer>
#include <QSocketNotifier>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QVariantMap>

#include "platform/can/VehicleCAN.h"
#include "platform/can/VehicleMessages.h"
#include "platform/types/VehicleTypes.h"
#include "platform/types/ModuleIDs.h"

class VehicleBusManager : public QObject
{
    Q_OBJECT

    // Connection state
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString interface READ interface NOTIFY interfaceChanged)

    // Module heartbeats
    Q_PROPERTY(bool ecmOnline READ ecmOnline NOTIFY moduleStatusChanged)
    Q_PROPERTY(bool tcmOnline READ tcmOnline NOTIFY moduleStatusChanged)
    Q_PROPERTY(bool pdcmOnline READ pdcmOnline NOTIFY moduleStatusChanged)
    Q_PROPERTY(bool gcmOnline READ gcmOnline NOTIFY moduleStatusChanged)

    // ECM telemetry — primary gauges
    Q_PROPERTY(int rpm READ rpm NOTIFY telemetryUpdated)
    Q_PROPERTY(double tps READ tps NOTIFY telemetryUpdated)
    Q_PROPERTY(int mapKpa READ mapKpa NOTIFY telemetryUpdated)
    Q_PROPERTY(int timing READ timing NOTIFY telemetryUpdated)
    Q_PROPERTY(double coolantTemp READ coolantTemp NOTIFY telemetryUpdated)
    Q_PROPERTY(double iatTemp READ iatTemp NOTIFY telemetryUpdated)
    Q_PROPERTY(double batteryVoltage READ batteryVoltage NOTIFY telemetryUpdated)
    Q_PROPERTY(int oilPressureKpa READ oilPressureKpa NOTIFY pressuresUpdated)
    Q_PROPERTY(int fuelPressureKpa READ fuelPressureKpa NOTIFY pressuresUpdated)

    // O2 / fueling
    Q_PROPERTY(double lambdaB1 READ lambdaB1 NOTIFY fuelUpdated)
    Q_PROPERTY(double lambdaB2 READ lambdaB2 NOTIFY fuelUpdated)
    Q_PROPERTY(double lambdaTarget READ lambdaTarget NOTIFY fuelUpdated)
    Q_PROPERTY(int stft READ stft NOTIFY fuelUpdated)
    Q_PROPERTY(int ltft READ ltft NOTIFY fuelUpdated)
    Q_PROPERTY(int injectorPwUs READ injectorPwUs NOTIFY fuelUpdated)
    Q_PROPERTY(double ve READ ve NOTIFY fuelUpdated)
    Q_PROPERTY(double injectorDuty READ injectorDuty NOTIFY fuelUpdated)

    // Ignition / knock
    Q_PROPERTY(int knockRetard READ knockRetard NOTIFY ignitionUpdated)
    Q_PROPERTY(int knockB1 READ knockB1 NOTIFY ignitionUpdated)
    Q_PROPERTY(int knockB2 READ knockB2 NOTIFY ignitionUpdated)

    // Drive mode
    Q_PROPERTY(int driveMode READ driveMode NOTIFY driveModeChanged)
    Q_PROPERTY(int launchState READ launchState NOTIFY driveModeChanged)
    Q_PROPERTY(int tractionMode READ tractionMode NOTIFY driveModeChanged)

    // Engine state
    Q_PROPERTY(int engineState READ engineState NOTIFY telemetryUpdated)

    // Faults
    Q_PROPERTY(int faultCount READ faultCount NOTIFY faultsUpdated)
    Q_PROPERTY(int sensorFaults READ sensorFaults NOTIFY faultsUpdated)

public:
    explicit VehicleBusManager(QObject *parent = nullptr);
    ~VehicleBusManager();

    // Property getters
    bool connected() const { return m_connected; }
    QString interface() const { return m_interface; }

    bool ecmOnline() const { return m_ecmOnline; }
    bool tcmOnline() const { return m_tcmOnline; }
    bool pdcmOnline() const { return m_pdcmOnline; }
    bool gcmOnline() const { return m_gcmOnline; }

    int rpm() const { return m_rpm; }
    double tps() const { return m_tps; }
    int mapKpa() const { return m_mapKpa; }
    int timing() const { return m_timing; }
    double coolantTemp() const { return m_coolantTemp; }
    double iatTemp() const { return m_iatTemp; }
    double batteryVoltage() const { return m_batteryVoltage; }
    int oilPressureKpa() const { return m_oilPressureKpa; }
    int fuelPressureKpa() const { return m_fuelPressureKpa; }

    double lambdaB1() const { return m_lambdaB1; }
    double lambdaB2() const { return m_lambdaB2; }
    double lambdaTarget() const { return m_lambdaTarget; }
    int stft() const { return m_stft; }
    int ltft() const { return m_ltft; }
    int injectorPwUs() const { return m_injectorPwUs; }
    double ve() const { return m_ve; }
    double injectorDuty() const { return m_injectorDuty; }

    int knockRetard() const { return m_knockRetard; }
    int knockB1() const { return m_knockB1; }
    int knockB2() const { return m_knockB2; }

    int driveMode() const { return m_driveMode; }
    int launchState() const { return m_launchState; }
    int tractionMode() const { return m_tractionMode; }

    int engineState() const { return m_engineState; }
    int faultCount() const { return m_faultCount; }
    int sensorFaults() const { return m_sensorFaults; }

    // Safe commands — callable from QML directly
    Q_INVOKABLE void sendGps(double lat, double lon, double alt, double speedKph, double heading, int fix, int sats);

    // Gated commands — require confirmation before CAN transmission
    // Step 1: QML calls request*() → sets pending state, emits confirmationRequired
    // Step 2: User confirms on screen → QML calls confirmPendingCommand()
    // Step 3: Actual CAN frame is sent
    Q_INVOKABLE void requestDriveMode(int mode);
    Q_INVOKABLE void requestLaunchControl(bool enable);
    Q_INVOKABLE void requestTractionControl(int mode);
    Q_INVOKABLE void requestTuneDelta(int tableId, int rpmIdx, int mapIdx, int delta);
    Q_INVOKABLE void requestTuneRollback();
    Q_INVOKABLE void confirmPendingCommand();
    Q_INVOKABLE void cancelPendingCommand();
    Q_INVOKABLE QString pendingCommandDescription() const;

    // NOT Q_INVOKABLE — only callable from C++ (confirmation gate or CAN gateway)
    void setDriveMode(int mode);
    void setLaunchControl(bool enable);
    void setTractionControl(int mode);
    void sendTuneDelta(int tableId, int rpmIdx, int mapIdx, int delta);
    void sendTuneRollback();

    // Connection control
    Q_INVOKABLE void connectBus(const QString &iface = "can0");
    Q_INVOKABLE void disconnectBus();

signals:
    void connectedChanged();
    void interfaceChanged();
    void moduleStatusChanged();
    void telemetryUpdated();
    void pressuresUpdated();
    void fuelUpdated();
    void ignitionUpdated();
    void driveModeChanged();
    void faultsUpdated();
    void tuneAckReceived(int tableId, int rpmIdx, int mapIdx, bool accepted);
    void confirmationRequired(const QString &description);  // QML shows confirmation dialog
    void pendingCommandCancelled();

private slots:
    void onCanData();
    void onHeartbeatTimer();
    void onModuleTimeoutCheck();
    void onUiUpdateTimer();  // 30Hz coalescing timer — emits dirty signals to QML

private:
    void processFrame(uint32_t canId, const uint8_t *data, uint8_t len);
    void sendFrame(uint32_t canId, const void *data, uint8_t len);
    void sendHeartbeat();

    // Socket
    int m_socket = -1;
    QSocketNotifier *m_notifier = nullptr;
    bool m_connected = false;
    QString m_interface;

    // Timers
    QTimer *m_heartbeatTimer;
    QTimer *m_timeoutTimer;

    // Module heartbeat tracking
    bool m_ecmOnline = false;
    bool m_tcmOnline = false;
    bool m_pdcmOnline = false;
    bool m_gcmOnline = false;
    QElapsedTimer m_ecmLastHeartbeat;
    QElapsedTimer m_tcmLastHeartbeat;
    QElapsedTimer m_pdcmLastHeartbeat;
    QElapsedTimer m_gcmLastHeartbeat;

    // ECM telemetry state
    int m_rpm = 0;
    double m_tps = 0.0;
    int m_mapKpa = 0;
    int m_timing = 0;
    double m_coolantTemp = 0.0;
    double m_iatTemp = 0.0;
    double m_batteryVoltage = 0.0;
    int m_oilPressureKpa = 0;
    int m_fuelPressureKpa = 0;

    double m_lambdaB1 = 0.0;
    double m_lambdaB2 = 0.0;
    double m_lambdaTarget = 0.0;
    int m_stft = 0;
    int m_ltft = 0;
    int m_injectorPwUs = 0;
    double m_ve = 0.0;
    double m_injectorDuty = 0.0;

    int m_knockRetard = 0;
    int m_knockB1 = 0;
    int m_knockB2 = 0;

    int m_driveMode = 1;  // NORMAL
    int m_launchState = 0;
    int m_tractionMode = 1;

    int m_engineState = 0;
    int m_faultCount = 0;
    int m_sensorFaults = 0;

    uint16_t m_tuneSeq = 0;

    // UI update coalescing — CAN data stored immediately, signals emitted at 30Hz max
    QTimer *m_uiUpdateTimer = nullptr;
    bool m_dirtyTelemetry = false;
    bool m_dirtyPressures = false;
    bool m_dirtyFuel = false;
    bool m_dirtyIgnition = false;
    bool m_dirtyDriveMode = false;
    bool m_dirtyFaults = false;
    bool m_dirtyModuleStatus = false;

    // Pending command gate — stores the command until user confirms
    enum class PendingCmd { None, DriveMode, LaunchControl, TractionControl, TuneDelta, TuneRollback };
    PendingCmd m_pendingCmd = PendingCmd::None;
    int m_pendingInt1 = 0, m_pendingInt2 = 0, m_pendingInt3 = 0, m_pendingInt4 = 0;
    bool m_pendingBool = false;
    QString m_pendingDescription;
};
