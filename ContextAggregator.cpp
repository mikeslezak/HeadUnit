#include "ContextAggregator.h"
#include "WeatherManager.h"
#include "VehicleBusManager.h"
#include "TidalClient.h"
#include "SpotifyClient.h"
#include "MediaController.h"
#include "BluetoothManager.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>

ContextAggregator::ContextAggregator(QObject *parent)
    : QObject(parent)
{
    qDebug() << "ContextAggregator: Initialized";
}

void ContextAggregator::setWeatherManager(WeatherManager *mgr) { m_weather = mgr; }
void ContextAggregator::setVehicleBusManager(VehicleBusManager *mgr) { m_vehicle = mgr; }
void ContextAggregator::setTidalClient(TidalClient *client) { m_tidal = client; }
void ContextAggregator::setSpotifyClient(SpotifyClient *client) { m_spotify = client; }
void ContextAggregator::setMediaController(MediaController *mgr) { m_media = mgr; }
void ContextAggregator::setBluetoothManager(BluetoothManager *mgr) { m_bluetooth = mgr; }

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

void ContextAggregator::setRouteCoordinates(const QJsonArray &coordinates, double /*durationSec*/)
{
    m_routeCoords = coordinates;
    qDebug() << "ContextAggregator: Stored" << coordinates.size() << "route coordinates for along-route search";
}

void ContextAggregator::clearRouteCoordinates()
{
    m_routeCoords = QJsonArray();
}

QList<QPair<double,double>> ContextAggregator::routeSamplePoints(int count) const
{
    QList<QPair<double,double>> points;
    if (m_routeCoords.isEmpty() || count <= 0) return points;

    int total = m_routeCoords.size();
    // Sample evenly spaced points along the route (skip start, include midpoints)
    for (int i = 1; i <= count; ++i) {
        int idx = (total * i) / (count + 1);
        if (idx >= total) idx = total - 1;
        QJsonArray coord = m_routeCoords[idx].toArray();
        if (coord.size() >= 2) {
            // GeoJSON is [lon, lat]
            points.append({coord[1].toDouble(), coord[0].toDouble()});
        }
    }
    return points;
}

double ContextAggregator::bestLatitude() const
{
    if (m_gpsLat != 0.0) return m_gpsLat;
    if (m_weather) return m_weather->latitude();
    return 0.0;
}

double ContextAggregator::bestLongitude() const
{
    if (m_gpsLon != 0.0) return m_gpsLon;
    if (m_weather) return m_weather->longitude();
    return 0.0;
}

QString ContextAggregator::buildContext() const
{
    QString ctx;

    // Time
    QDateTime now = QDateTime::currentDateTime();
    ctx += QString("Current time: %1\n").arg(now.toString("dddd, MMMM d yyyy, h:mm AP"));

    // GPS location (prefer real GPS, fall back to WeatherManager's ip-api geolocation)
    double lat = m_gpsLat;
    double lon = m_gpsLon;
    bool fromGps = true;
    if (lat == 0.0 && lon == 0.0 && m_weather) {
        lat = m_weather->latitude();
        lon = m_weather->longitude();
        fromGps = false;
    }
    if (lat != 0.0 || lon != 0.0) {
        ctx += QString("GPS coordinates: %1, %2%3\n")
            .arg(lat, 0, 'f', 5).arg(lon, 0, 'f', 5)
            .arg(fromGps ? "" : " (approximate, from IP geolocation)");
        if (fromGps && m_gpsSpeed > 0.0) {
            ctx += QString("Speed: %1 km/h (%2 mph)\n")
                .arg(m_gpsSpeed, 0, 'f', 0)
                .arg(m_gpsSpeed * 0.621371, 0, 'f', 0);
        }
        if (fromGps && m_gpsHeading >= 0.0) {
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

    // Phone status
    if (m_bluetooth) {
        int battery = m_bluetooth->phoneBatteryLevel();
        int signal = m_bluetooth->cellularSignal();
        QString carrier = m_bluetooth->carrierName();
        QString roaming = m_bluetooth->roamingStatus();

        if (battery >= 0 || signal > 0 || !carrier.isEmpty()) {
            ctx += QString("Phone: %1 battery, %2/4 signal bars")
                .arg(battery >= 0 ? QString("%1%").arg(battery) : "unknown")
                .arg(signal);
            if (!carrier.isEmpty()) ctx += QString(", carrier %1").arg(carrier);
            if (roaming == "roaming") ctx += " (ROAMING)";
            ctx += "\n";
        }

        if (m_bluetooth->hasActiveCall()) {
            QString caller = m_bluetooth->activeCallName().isEmpty()
                ? m_bluetooth->activeCallNumber() : m_bluetooth->activeCallName();
            ctx += QString("Active call: %1 (%2), duration %3s\n")
                .arg(caller, m_bluetooth->activeCallState())
                .arg(m_bluetooth->activeCallDuration());
        }
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

    // Music (playing or paused — track is loaded either way)
    if (m_tidal && !m_tidal->trackTitle().isEmpty()) {
        ctx += QString("Music (%1, Tidal): \"%2\" by %3")
            .arg(m_tidal->isPlaying() ? "playing" : "paused",
                 m_tidal->trackTitle(), m_tidal->artist());
        if (!m_tidal->album().isEmpty())
            ctx += QString(" from %1").arg(m_tidal->album());
        ctx += "\n";
    } else if (m_spotify && !m_spotify->trackTitle().isEmpty()) {
        ctx += QString("Music (%1, Spotify): \"%2\" by %3")
            .arg(m_spotify->isPlaying() ? "playing" : "paused",
                 m_spotify->trackTitle(), m_spotify->artist());
        if (!m_spotify->album().isEmpty())
            ctx += QString(" from %1").arg(m_spotify->album());
        ctx += "\n";
    } else if (m_media && !m_media->trackTitle().isEmpty()) {
        ctx += QString("Music (%1, Bluetooth): \"%2\" by %3\n")
            .arg(m_media->isPlaying() ? "playing" : "paused",
                 m_media->trackTitle(), m_media->artist());
    }

    return ctx;
}
