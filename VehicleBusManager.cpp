#include "VehicleBusManager.h"
#include <QDebug>
#include <cstring>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>

// CAN FD support
#ifndef CANFD_MTU
#include <linux/can/raw.h>
#endif

VehicleBusManager::VehicleBusManager(QObject *parent)
    : QObject(parent)
    , m_heartbeatTimer(new QTimer(this))
    , m_timeoutTimer(new QTimer(this))
{
    // HMI heartbeat at 1000ms per platform spec
    m_heartbeatTimer->setInterval(VEH_RATE_HEARTBEAT_MODULE);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &VehicleBusManager::onHeartbeatTimer);

    // Check module timeouts every 500ms
    m_timeoutTimer->setInterval(500);
    connect(m_timeoutTimer, &QTimer::timeout, this, &VehicleBusManager::onModuleTimeoutCheck);

    qDebug() << "VehicleBusManager: Initialized (not connected)";
}

VehicleBusManager::~VehicleBusManager()
{
    disconnectBus();
}

void VehicleBusManager::connectBus(const QString &iface)
{
    if (m_connected) {
        qWarning() << "VehicleBusManager: Already connected to" << m_interface;
        return;
    }

    m_interface = iface;

    // Create SocketCAN socket (CAN FD)
    m_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socket < 0) {
        qWarning() << "VehicleBusManager: Failed to create CAN socket:" << strerror(errno);
        return;
    }

    // Enable CAN FD
    int enable_canfd = 1;
    if (setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_canfd, sizeof(enable_canfd)) < 0) {
        qWarning() << "VehicleBusManager: CAN FD not supported, falling back to classic CAN";
    }

    // Bind to interface
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface.toUtf8().constData(), IFNAMSIZ - 1);

    if (ioctl(m_socket, SIOCGIFINDEX, &ifr) < 0) {
        qWarning() << "VehicleBusManager: Interface" << iface << "not found:" << strerror(errno);
        ::close(m_socket);
        m_socket = -1;
        return;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(m_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        qWarning() << "VehicleBusManager: Failed to bind CAN socket:" << strerror(errno);
        ::close(m_socket);
        m_socket = -1;
        return;
    }

    // Set up Qt socket notifier for async reads
    m_notifier = new QSocketNotifier(m_socket, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &VehicleBusManager::onCanData);

    m_connected = true;
    emit connectedChanged();
    emit interfaceChanged();

    // Start heartbeat and timeout monitoring
    m_heartbeatTimer->start();
    m_timeoutTimer->start();

    qDebug() << "VehicleBusManager: Connected to" << iface;

    // Send initial heartbeat
    sendHeartbeat();
}

void VehicleBusManager::disconnectBus()
{
    m_heartbeatTimer->stop();
    m_timeoutTimer->stop();

    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }

    if (m_socket >= 0) {
        ::close(m_socket);
        m_socket = -1;
    }

    if (m_connected) {
        m_connected = false;
        m_ecmOnline = false;
        m_tcmOnline = false;
        m_pdcmOnline = false;
        m_gcmOnline = false;
        emit connectedChanged();
        emit moduleStatusChanged();
        qDebug() << "VehicleBusManager: Disconnected";
    }
}

// ============================================================================
// CAN Frame I/O
// ============================================================================

void VehicleBusManager::onCanData()
{
    struct canfd_frame frame;
    ssize_t nbytes = read(m_socket, &frame, sizeof(frame));

    if (nbytes < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            qWarning() << "VehicleBusManager: Read error:" << strerror(errno);
        }
        return;
    }

    // Determine payload length
    uint8_t len = (nbytes == CANFD_MTU) ? frame.len : frame.len;

    processFrame(frame.can_id & CAN_EFF_MASK, frame.data, len);
}

void VehicleBusManager::sendFrame(uint32_t canId, const void *data, uint8_t len)
{
    if (m_socket < 0) return;

    if (len <= 8) {
        // Classic CAN frame
        struct can_frame frame;
        memset(&frame, 0, sizeof(frame));
        frame.can_id = canId;
        frame.can_dlc = len;
        memcpy(frame.data, data, len);

        if (write(m_socket, &frame, sizeof(frame)) < 0) {
            qWarning() << "VehicleBusManager: Send failed for 0x" << Qt::hex << canId << ":" << strerror(errno);
        }
    } else {
        // CAN FD frame (>8 bytes)
        struct canfd_frame frame;
        memset(&frame, 0, sizeof(frame));
        frame.can_id = canId;
        frame.flags = CANFD_BRS;  // Bit rate switch for data phase
        frame.len = len;
        memcpy(frame.data, data, len);

        if (write(m_socket, &frame, sizeof(frame)) < 0) {
            qWarning() << "VehicleBusManager: Send FD failed for 0x" << Qt::hex << canId << ":" << strerror(errno);
        }
    }
}

// ============================================================================
// Message Processing
// ============================================================================

void VehicleBusManager::processFrame(uint32_t canId, const uint8_t *data, uint8_t len)
{
    switch (canId) {

    // ---- ECM Telemetry ----

    case CAN_VEH_ECM_RPM_TPS_MAP: {
        if (len < sizeof(VehMsgRpmTpsMap)) break;
        const auto *msg = reinterpret_cast<const VehMsgRpmTpsMap *>(data);
        m_rpm = msg->rpm;
        m_tps = msg->tps / 10.0;
        m_mapKpa = msg->map_kpa;
        m_timing = msg->timing;
        emit telemetryUpdated();
        break;
    }

    case CAN_VEH_ECM_TEMPS_BATT: {
        if (len < sizeof(VehMsgTempsBatt)) break;
        const auto *msg = reinterpret_cast<const VehMsgTempsBatt *>(data);
        m_coolantTemp = msg->coolant / 10.0;
        m_iatTemp = msg->iat / 10.0;
        m_batteryVoltage = msg->battery_mv / 1000.0;
        emit telemetryUpdated();
        break;
    }

    case CAN_VEH_ECM_O2_FUELTRIM: {
        if (len < sizeof(VehMsgO2FuelTrim)) break;
        const auto *msg = reinterpret_cast<const VehMsgO2FuelTrim *>(data);
        m_lambdaB1 = msg->lambda_b1 / 1000.0;
        m_lambdaB2 = msg->lambda_b2 / 1000.0;
        m_lambdaTarget = msg->lambda_target / 1000.0;
        m_stft = msg->stft;
        m_ltft = msg->ltft;
        emit fuelUpdated();
        break;
    }

    case CAN_VEH_ECM_IGN_KNOCK: {
        if (len < sizeof(VehMsgIgnKnock)) break;
        const auto *msg = reinterpret_cast<const VehMsgIgnKnock *>(data);
        m_timing = msg->timing;
        m_knockRetard = msg->knock_retard;
        m_knockB1 = msg->knock_b1;
        m_knockB2 = msg->knock_b2;
        emit ignitionUpdated();
        break;
    }

    case CAN_VEH_ECM_PRESSURES: {
        if (len < sizeof(VehMsgPressures)) break;
        const auto *msg = reinterpret_cast<const VehMsgPressures *>(data);
        m_oilPressureKpa = msg->oil_kpa;
        m_fuelPressureKpa = msg->fuel_kpa;
        emit pressuresUpdated();
        break;
    }

    case CAN_VEH_ECM_INJ_VE: {
        if (len < sizeof(VehMsgInjVe)) break;
        const auto *msg = reinterpret_cast<const VehMsgInjVe *>(data);
        m_injectorPwUs = msg->injector_pw_us;
        m_ve = msg->ve_pct / 10.0;
        m_engineState = msg->engine_state;
        m_injectorDuty = msg->duty_cycle / 10.0;
        emit fuelUpdated();
        break;
    }

    case CAN_VEH_ECM_FAULTS: {
        if (len < sizeof(VehMsgFaults)) break;
        const auto *msg = reinterpret_cast<const VehMsgFaults *>(data);
        m_sensorFaults = msg->sensor_faults;
        m_faultCount = msg->fault_count;
        emit faultsUpdated();
        break;
    }

    case CAN_VEH_ECM_DRIVE_MODE: {
        if (len < sizeof(VehMsgDriveMode)) break;
        const auto *msg = reinterpret_cast<const VehMsgDriveMode *>(data);
        m_driveMode = msg->drive_mode;
        m_launchState = msg->launch_state;
        m_tractionMode = msg->traction_mode;
        emit driveModeChanged();
        break;
    }

    // ---- Tune ACK from ECM ----

    case CAN_VEH_HMI_TUNE_ACK: {
        if (len < sizeof(VehMsgTuneDelta)) break;
        const auto *msg = reinterpret_cast<const VehMsgTuneDelta *>(data);
        emit tuneAckReceived(msg->table_id, msg->rpm_idx, msg->map_idx, msg->delta != 0);
        break;
    }

    // ---- Heartbeats ----

    case CAN_VEH_ECM_HEARTBEAT: {
        m_ecmLastHeartbeat.restart();
        if (!m_ecmOnline) {
            m_ecmOnline = true;
            emit moduleStatusChanged();
            qDebug() << "VehicleBusManager: ECM online";
        }
        break;
    }

    case CAN_VEH_TCM_HEARTBEAT: {
        m_tcmLastHeartbeat.restart();
        if (!m_tcmOnline) {
            m_tcmOnline = true;
            emit moduleStatusChanged();
            qDebug() << "VehicleBusManager: TCM online";
        }
        break;
    }

    case CAN_VEH_PDCM_HEARTBEAT: {
        m_pdcmLastHeartbeat.restart();
        if (!m_pdcmOnline) {
            m_pdcmOnline = true;
            emit moduleStatusChanged();
            qDebug() << "VehicleBusManager: PDCM online";
        }
        break;
    }

    case CAN_VEH_GCM_HEARTBEAT: {
        m_gcmLastHeartbeat.restart();
        if (!m_gcmOnline) {
            m_gcmOnline = true;
            emit moduleStatusChanged();
            qDebug() << "VehicleBusManager: GCM online";
        }
        break;
    }

    default:
        // Unhandled message — ignore silently
        break;
    }
}

// ============================================================================
// Heartbeat & Timeout
// ============================================================================

void VehicleBusManager::onHeartbeatTimer()
{
    sendHeartbeat();
}

void VehicleBusManager::sendHeartbeat()
{
    VehMsgHeartbeat hb;
    hb.module_id = ModuleID::HMI;
    hb.status = static_cast<uint8_t>(HeartbeatStatus::OK);
    sendFrame(CAN_VEH_HMI_HEARTBEAT, &hb, sizeof(hb));
}

void VehicleBusManager::onModuleTimeoutCheck()
{
    bool changed = false;

    if (m_ecmOnline && m_ecmLastHeartbeat.elapsed() > VEH_HEARTBEAT_TIMEOUT_MS) {
        m_ecmOnline = false;
        changed = true;
        qWarning() << "VehicleBusManager: ECM offline (heartbeat timeout)";
    }
    if (m_tcmOnline && m_tcmLastHeartbeat.elapsed() > VEH_HEARTBEAT_TIMEOUT_MS) {
        m_tcmOnline = false;
        changed = true;
        qWarning() << "VehicleBusManager: TCM offline (heartbeat timeout)";
    }
    if (m_pdcmOnline && m_pdcmLastHeartbeat.elapsed() > VEH_HEARTBEAT_TIMEOUT_MS) {
        m_pdcmOnline = false;
        changed = true;
        qWarning() << "VehicleBusManager: PDCM offline (heartbeat timeout)";
    }
    if (m_gcmOnline && m_gcmLastHeartbeat.elapsed() > VEH_HEARTBEAT_TIMEOUT_MS) {
        m_gcmOnline = false;
        changed = true;
        qWarning() << "VehicleBusManager: GCM offline (heartbeat timeout)";
    }

    if (changed) emit moduleStatusChanged();
}

// ============================================================================
// HMI Commands → Vehicle Bus
// ============================================================================

void VehicleBusManager::setDriveMode(int mode)
{
    VehMsgDriveModeCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.mode = static_cast<uint8_t>(mode);
    cmd.source = 0;  // HMI source
    sendFrame(CAN_VEH_HMI_DRIVE_MODE_CMD, &cmd, sizeof(cmd));
    qDebug() << "VehicleBusManager: Drive mode command sent:" << mode;
}

void VehicleBusManager::setLaunchControl(bool enable)
{
    uint8_t data[4] = {};
    data[0] = enable ? 1 : 0;
    sendFrame(CAN_VEH_HMI_LAUNCH_CMD, data, 4);
    qDebug() << "VehicleBusManager: Launch control" << (enable ? "enabled" : "disabled");
}

void VehicleBusManager::setTractionControl(int mode)
{
    uint8_t data[4] = {};
    data[0] = static_cast<uint8_t>(mode);
    sendFrame(CAN_VEH_HMI_TRACTION_CMD, data, 4);
    qDebug() << "VehicleBusManager: Traction control mode:" << mode;
}

void VehicleBusManager::sendGps(double lat, double lon, double alt, double speedKph, double heading, int fix, int sats)
{
    VehMsgGpsAltitude msg;
    memset(&msg, 0, sizeof(msg));
    msg.latitude = static_cast<int32_t>(lat * 1e7);
    msg.longitude = static_cast<int32_t>(lon * 1e7);
    msg.altitude_m = static_cast<int16_t>(alt);
    msg.speed_kph = static_cast<uint16_t>(speedKph * 10);
    msg.fix_quality = static_cast<uint8_t>(fix);
    msg.satellites = static_cast<uint8_t>(sats);
    msg.heading = static_cast<uint16_t>(heading * 10);
    sendFrame(CAN_VEH_HMI_GPS_ALTITUDE, &msg, sizeof(msg));
}

void VehicleBusManager::sendTuneDelta(int tableId, int rpmIdx, int mapIdx, int delta)
{
    if (!m_ecmOnline) {
        qWarning() << "VehicleBusManager: Cannot send tune delta - ECM offline";
        return;
    }

    VehMsgTuneDelta msg;
    memset(&msg, 0, sizeof(msg));
    msg.table_id = static_cast<uint8_t>(tableId);
    msg.rpm_idx = static_cast<uint8_t>(rpmIdx);
    msg.map_idx = static_cast<uint8_t>(mapIdx);
    msg.delta = static_cast<int16_t>(delta);
    msg.session_seq = m_tuneSeq++;
    sendFrame(CAN_VEH_HMI_AI_TUNE_DELTA, &msg, sizeof(msg));
    qDebug() << "VehicleBusManager: Tune delta sent - table:" << tableId
             << "rpm:" << rpmIdx << "map:" << mapIdx << "delta:" << delta;
}

void VehicleBusManager::sendTuneRollback()
{
    uint8_t data[4] = {};
    data[0] = 0xFF;  // Rollback all
    sendFrame(CAN_VEH_HMI_AI_ROLLBACK, data, 4);
    m_tuneSeq = 0;
    qDebug() << "VehicleBusManager: Rollback command sent";
}
