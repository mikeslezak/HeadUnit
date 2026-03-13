#include "ContextAggregator.h"
#include "WeatherManager.h"
#include "VehicleBusManager.h"
#include <QDateTime>
#include <QDebug>

ContextAggregator::ContextAggregator(QObject *parent)
    : QObject(parent)
{
    qDebug() << "ContextAggregator: Initialized";
}

void ContextAggregator::setWeatherManager(WeatherManager *mgr) { m_weather = mgr; }
void ContextAggregator::setVehicleBusManager(VehicleBusManager *mgr) { m_vehicle = mgr; }

void ContextAggregator::setGpsLatitude(double lat)
{
    if (qFuzzyCompare(m_gpsLat, lat)) return;
    m_gpsLat = lat;
    emit gpsChanged();
}

void ContextAggregator::setGpsLongitude(double lon)
{
    if (qFuzzyCompare(m_gpsLon, lon)) return;
    m_gpsLon = lon;
    emit gpsChanged();
}

void ContextAggregator::setGpsSpeed(double speed)
{
    m_gpsSpeed = speed;
    emit gpsChanged();
}

void ContextAggregator::setGpsHeading(double heading)
{
    m_gpsHeading = heading;
    emit gpsChanged();
}

void ContextAggregator::setRouteActive(bool active)
{
    if (m_routeActive == active) return;
    m_routeActive = active;
    emit routeChanged();
}

void ContextAggregator::setRouteDestination(const QString &dest)
{
    m_routeDest = dest;
    emit routeChanged();
}

void ContextAggregator::setRouteDistance(const QString &dist)
{
    m_routeDist = dist;
    emit routeChanged();
}

void ContextAggregator::setRouteDuration(const QString &dur)
{
    m_routeDur = dur;
    emit routeChanged();
}

void ContextAggregator::setRouteWeatherSummary(const QString &summary)
{
    m_routeWeatherSummary = summary;
}

void ContextAggregator::setRoadConditionsSummary(const QString &summary)
{
    m_roadConditionsSummary = summary;
}

void ContextAggregator::setSpeedLimitSummary(const QString &summary)
{
    m_speedLimitSummary = summary;
}

void ContextAggregator::setRoadSurfaceSummary(const QString &summary)
{
    m_roadSurfaceSummary = summary;
}

void ContextAggregator::setAvalancheSummary(const QString &summary)
{
    m_avalancheSummary = summary;
}

void ContextAggregator::setBorderWaitSummary(const QString &summary)
{
    m_borderWaitSummary = summary;
}

QString ContextAggregator::buildContext() const
{
    QString ctx;

    // Time
    QDateTime now = QDateTime::currentDateTime();
    ctx += QString("Current time: %1\n").arg(now.toString("dddd, MMMM d yyyy, h:mm AP"));

    // GPS location
    if (m_gpsLat != 0.0 || m_gpsLon != 0.0) {
        ctx += QString("GPS coordinates: %1, %2\n").arg(m_gpsLat, 0, 'f', 5).arg(m_gpsLon, 0, 'f', 5);
        if (m_gpsSpeed > 0.0) {
            ctx += QString("Speed: %1 km/h (%2 mph)\n")
                .arg(m_gpsSpeed, 0, 'f', 0)
                .arg(m_gpsSpeed * 0.621371, 0, 'f', 0);
        }
        if (m_gpsHeading >= 0.0) {
            ctx += QString("Heading: %1°\n").arg(m_gpsHeading, 0, 'f', 0);
        }
    }

    // Weather
    if (m_weather) {
        QString loc = m_weather->locationName();
        if (!loc.isEmpty()) {
            ctx += QString("Location: %1\n").arg(loc);
        }
        ctx += QString("Weather: %1, %2°C (feels like %3°C), humidity %4%, wind %5 km/h\n")
            .arg(m_weather->weatherDescription())
            .arg(m_weather->temperature(), 0, 'f', 1)
            .arg(m_weather->feelsLike(), 0, 'f', 1)
            .arg(m_weather->humidity())
            .arg(m_weather->windSpeed(), 0, 'f', 1);
    }

    // Vehicle state
    if (m_vehicle && m_vehicle->connected()) {
        ctx += QString("Vehicle: RPM %1, coolant %2°C, battery %3V")
            .arg(m_vehicle->rpm())
            .arg(m_vehicle->coolantTemp(), 0, 'f', 0)
            .arg(m_vehicle->batteryVoltage(), 0, 'f', 1);
        if (m_vehicle->faultCount() > 0) {
            ctx += QString(", %1 faults active").arg(m_vehicle->faultCount());
        }
        ctx += "\n";
    }

    // Active route
    if (m_routeActive) {
        ctx += QString("Active route: destination %1, distance %2, ETA %3\n")
            .arg(m_routeDest, m_routeDist, m_routeDur);
    }

    // Route weather (from RouteWeatherManager)
    if (!m_routeWeatherSummary.isEmpty()) {
        ctx += "Weather along route:\n" + m_routeWeatherSummary + "\n";
    }

    // Road conditions (from RoadConditionManager)
    if (!m_roadConditionsSummary.isEmpty()) {
        ctx += "Road conditions along route:\n" + m_roadConditionsSummary + "\n";
    }

    // Speed limits (from SpeedLimitManager)
    if (!m_speedLimitSummary.isEmpty()) {
        ctx += "Speed limits: " + m_speedLimitSummary + "\n";
    }

    // Road surface conditions (from RoadSurfaceManager)
    if (!m_roadSurfaceSummary.isEmpty()) {
        ctx += "Road surface conditions:\n" + m_roadSurfaceSummary + "\n";
    }

    // Avalanche conditions (from AvalancheManager)
    if (!m_avalancheSummary.isEmpty()) {
        ctx += "Avalanche conditions:\n" + m_avalancheSummary + "\n";
    }

    // Border crossing wait times (from BorderWaitManager)
    if (!m_borderWaitSummary.isEmpty()) {
        ctx += "Border crossing wait times:\n" + m_borderWaitSummary + "\n";
    }

    return ctx;
}
