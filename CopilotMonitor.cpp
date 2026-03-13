#include "CopilotMonitor.h"
#include "ContextAggregator.h"
#include "VehicleBusManager.h"
#include "RouteWeatherManager.h"
#include "RoadConditionManager.h"
#include "SpeedLimitManager.h"
#include "RoadSurfaceManager.h"
#include "AvalancheManager.h"
#include "BorderWaitManager.h"
#include <QDebug>
#include <QDateTime>

CopilotMonitor::CopilotMonitor(QObject *parent)
    : QObject(parent)
    , m_checkTimer(new QTimer(this))
{
    // Check every 60 seconds
    m_checkTimer->setInterval(60 * 1000);
    connect(m_checkTimer, &QTimer::timeout, this, &CopilotMonitor::checkConditions);
    m_checkTimer->start();

    qDebug() << "CopilotMonitor: Initialized";
}

void CopilotMonitor::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }
void CopilotMonitor::setVehicleBusManager(VehicleBusManager *vehicle) { m_vehicle = vehicle; }
void CopilotMonitor::setRouteWeatherManager(RouteWeatherManager *routeWeather)
{
    m_routeWeather = routeWeather;
    if (m_routeWeather) {
        connect(m_routeWeather, &RouteWeatherManager::alertDetected,
                this, &CopilotMonitor::onRouteWeatherAlert);
    }
}

void CopilotMonitor::setEnabled(bool on)
{
    if (m_enabled == on) return;
    m_enabled = on;
    emit enabledChanged();
}

void CopilotMonitor::setQuietMode(bool quiet)
{
    if (m_quietMode == quiet) return;
    m_quietMode = quiet;
    emit quietModeChanged();
    qDebug() << "CopilotMonitor: Quiet mode" << (quiet ? "enabled" : "disabled");
}

void CopilotMonitor::checkConditions()
{
    if (!m_enabled || m_quietMode) return;

    // === Vehicle fault check ===
    if (m_vehicle && m_vehicle->connected()) {
        if (m_vehicle->faultCount() > 0) {
            emitAlert("vehicle_fault",
                QString("Vehicle diagnostic: %1 fault%2 detected")
                    .arg(m_vehicle->faultCount())
                    .arg(m_vehicle->faultCount() > 1 ? "s" : ""));
        }

        // Coolant temperature warning (105°C = 221°F is hot)
        if (m_vehicle->coolantTemp() > 105.0) {
            emitAlert("coolant_high",
                QString("Coolant temperature is high at %1 degrees")
                    .arg(qRound(m_vehicle->coolantTemp())));
        }
    }

    // === Driving duration check ===
    if (m_context) {
        bool moving = m_context->gpsSpeed() > 10.0;  // > 10 km/h = driving

        if (moving && !m_isDriving) {
            m_isDriving = true;
            m_drivingTimer.start();
            m_drivingAlertSent = false;
        } else if (!moving && m_isDriving) {
            // Stopped — reset if stopped for > 5 min (checked next cycle)
            if (m_drivingTimer.elapsed() > 5 * 60 * 1000) {
                m_isDriving = false;
                m_drivingAlertSent = false;
            }
        }

        if (m_isDriving && !m_drivingAlertSent && m_drivingTimer.elapsed() > 2 * 3600 * 1000) {
            emitAlert("long_drive",
                "You've been driving for over 2 hours. Want me to find a rest stop?");
            m_drivingAlertSent = true;
        }
    }
}

void CopilotMonitor::setRoadConditionManager(RoadConditionManager *roadConditions)
{
    m_roadConditions = roadConditions;
    if (m_roadConditions) {
        connect(m_roadConditions, &RoadConditionManager::alertDetected,
                this, &CopilotMonitor::onRoadConditionAlert);
    }
}

void CopilotMonitor::onRouteWeatherAlert(const QString &message)
{
    if (!m_enabled || m_quietMode) return;
    emitAlert("route_weather", message);
}

void CopilotMonitor::onRoadConditionAlert(const QString &message)
{
    if (!m_enabled || m_quietMode) return;
    emitAlert("road_condition", message);
}

void CopilotMonitor::setSpeedLimitManager(SpeedLimitManager *speedLimit)
{
    m_speedLimit = speedLimit;
    if (m_speedLimit) {
        connect(m_speedLimit, &SpeedLimitManager::alertDetected,
                this, &CopilotMonitor::onSpeedLimitAlert);
    }
}

void CopilotMonitor::setRoadSurfaceManager(RoadSurfaceManager *roadSurface)
{
    m_roadSurface = roadSurface;
    if (m_roadSurface) {
        connect(m_roadSurface, &RoadSurfaceManager::alertDetected,
                this, &CopilotMonitor::onRoadSurfaceAlert);
    }
}

void CopilotMonitor::setAvalancheManager(AvalancheManager *avalanche)
{
    m_avalanche = avalanche;
    if (m_avalanche) {
        connect(m_avalanche, &AvalancheManager::alertDetected,
                this, &CopilotMonitor::onAvalancheAlert);
    }
}

void CopilotMonitor::setBorderWaitManager(BorderWaitManager *borderWait)
{
    m_borderWait = borderWait;
    if (m_borderWait) {
        connect(m_borderWait, &BorderWaitManager::alertDetected,
                this, &CopilotMonitor::onBorderWaitAlert);
    }
}

void CopilotMonitor::onSpeedLimitAlert(const QString &message)
{
    if (!m_enabled || m_quietMode) return;
    emitAlert("speed_limit", message);
}

void CopilotMonitor::onRoadSurfaceAlert(const QString &message)
{
    if (!m_enabled || m_quietMode) return;
    emitAlert("road_surface", message);
}

void CopilotMonitor::onAvalancheAlert(const QString &message)
{
    if (!m_enabled || m_quietMode) return;
    emitAlert("avalanche", message);
}

void CopilotMonitor::onBorderWaitAlert(const QString &message)
{
    if (!m_enabled || m_quietMode) return;
    emitAlert("border_wait", message);
}

bool CopilotMonitor::shouldThrottle(const QString &alertType)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Global throttle: max 1 alert per 10 minutes
    if (now - m_lastAnyAlertTime < MIN_ALERT_INTERVAL_MS) {
        return true;
    }

    // Per-type throttle: same alert type suppressed for 30 minutes
    if (m_lastAlertTime.contains(alertType)) {
        if (now - m_lastAlertTime[alertType] < ALERT_COOLDOWN_MS) {
            return true;
        }
    }

    return false;
}

void CopilotMonitor::emitAlert(const QString &alertType, const QString &message)
{
    if (shouldThrottle(alertType)) {
        qDebug() << "CopilotMonitor: Throttled alert:" << alertType;
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastAlertTime[alertType] = now;
    m_lastAnyAlertTime = now;

    qDebug() << "CopilotMonitor: Alert -" << alertType << ":" << message;
    emit proactiveAlert(message);
}
