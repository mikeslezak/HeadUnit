#include "RouteWeatherManager.h"
#include "ContextAggregator.h"
#include "GeoUtils.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QtMath>
#include <QDateTime>

RouteWeatherManager::RouteWeatherManager(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    connect(m_network, &QNetworkAccessManager::finished,
            this, &RouteWeatherManager::onWeatherReply);

    m_refreshTimer->setInterval(REFRESH_INTERVAL_MS);
    connect(m_refreshTimer, &QTimer::timeout, this, &RouteWeatherManager::refreshForecasts);

    qDebug() << "RouteWeatherManager: Initialized (30min refresh, 2hr lookahead, 15km sampling)";
}

void RouteWeatherManager::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }

void RouteWeatherManager::setRouteCoordinates(const QJsonArray &coordinates, double durationSec)
{
    if (coordinates.size() < 2) {
        clearRoute();
        return;
    }

    ++m_generation;
    m_routeCoordinates = coordinates;
    m_totalDurationSec = durationSec;
    sampleRoutePoints(coordinates, durationSec);

    m_active = true;
    emit activeChanged();

    fetchWeather();
    m_refreshTimer->start();

    qDebug() << "RouteWeatherManager: Tracking" << m_points.size()
             << "points along route (2hr lookahead)";
}

void RouteWeatherManager::clearRoute()
{
    ++m_generation;
    m_active = false;
    m_routeCoordinates = QJsonArray();
    m_points.clear();
    m_summary.clear();
    m_refreshTimer->stop();
    emit activeChanged();
    emit summaryChanged();

    if (m_context) {
        m_context->setRouteWeatherSummary(QString());
    }
    qDebug() << "RouteWeatherManager: Route cleared";
}

void RouteWeatherManager::sampleRoutePoints(const QJsonArray &coordinates, double durationSec)
{
    m_points.clear();

    int numCoords = coordinates.size();
    if (numCoords < 2) return;

    // Walk the route, accumulating distance, and drop a sample point every SAMPLE_INTERVAL_KM.
    // Stop at the 2-hour lookahead mark.
    double totalDistKm = 0.0;
    double lastSampleDist = -SAMPLE_INTERVAL_KM; // force first point

    // First pass: compute total route distance for speed estimation
    double routeTotalKm = 0.0;
    double prevLat = 0, prevLon = 0;
    for (int i = 0; i < numCoords; ++i) {
        QJsonArray c = coordinates[i].toArray();
        if (c.size() < 2) continue;
        double lat = c[1].toDouble();
        double lon = c[0].toDouble();
        if (i > 0) {
            routeTotalKm += GeoUtils::haversineKm(prevLat, prevLon, lat, lon);
        }
        prevLat = lat;
        prevLon = lon;
    }

    double avgSpeedKmh = (durationSec > 0) ? (routeTotalKm / (durationSec / 3600.0)) : 80.0;
    double lookaheadKm = avgSpeedKmh * LOOKAHEAD_HOURS;

    qDebug() << "RouteWeatherManager: Route" << routeTotalKm << "km, avg speed"
             << avgSpeedKmh << "km/h, lookahead" << lookaheadKm << "km";

    // Second pass: sample points
    prevLat = 0; prevLon = 0;
    totalDistKm = 0.0;
    for (int i = 0; i < numCoords; ++i) {
        QJsonArray c = coordinates[i].toArray();
        if (c.size() < 2) continue;
        double lat = c[1].toDouble();
        double lon = c[0].toDouble();

        if (i > 0) {
            totalDistKm += GeoUtils::haversineKm(prevLat, prevLon, lat, lon);
        }
        prevLat = lat;
        prevLon = lon;

        // Stop if beyond 2-hour lookahead
        if (totalDistKm > lookaheadKm) break;

        // Sample at every SAMPLE_INTERVAL_KM
        if (totalDistKm - lastSampleDist >= SAMPLE_INTERVAL_KM || i == 0) {
            RoutePoint pt;
            pt.lat = lat;
            pt.lon = lon;
            pt.etaMinutes = (avgSpeedKmh > 0) ? (totalDistKm / avgSpeedKmh) * 60.0 : 0.0;

            if (i == 0) {
                pt.locationLabel = "Current location";
            } else {
                int mins = qRound(pt.etaMinutes);
                if (mins < 60) {
                    pt.locationLabel = QString("%1 min ahead").arg(mins);
                } else {
                    pt.locationLabel = QString("%1h %2m ahead")
                        .arg(mins / 60).arg(mins % 60);
                }
            }

            m_points.append(pt);
            lastSampleDist = totalDistKm;
        }
    }

    qDebug() << "RouteWeatherManager: Sampled" << m_points.size()
             << "points over" << qMin(totalDistKm, lookaheadKm) << "km";
}

void RouteWeatherManager::fetchWeather()
{
    if (m_points.isEmpty()) return;

    // Build comma-separated lat/lon lists for multi-coordinate Open-Meteo request
    QStringList lats, lons;
    for (const auto &pt : m_points) {
        lats.append(QString::number(pt.lat, 'f', 4));
        lons.append(QString::number(pt.lon, 'f', 4));
    }

    // Open-Meteo with minutely_15 for high-res precipitation (HRRR model for North America)
    QString url = "https://api.open-meteo.com/v1/forecast";
    QUrlQuery params;
    params.addQueryItem("latitude", lats.join(","));
    params.addQueryItem("longitude", lons.join(","));
    params.addQueryItem("minutely_15", "precipitation,weather_code,temperature_2m,wind_speed_10m");
    params.addQueryItem("forecast_minutely_15", "24"); // 24 steps = 6 hours of 15-min data
    params.addQueryItem("timezone", "auto");

    QUrl requestUrl(url);
    requestUrl.setQuery(params);

    QNetworkRequest req(requestUrl);
    req.setAttribute(QNetworkRequest::UserMax, m_generation);

    qDebug() << "RouteWeatherManager: Fetching weather for" << m_points.size() << "points";
    m_network->get(req);
}

void RouteWeatherManager::onWeatherReply(QNetworkReply *reply)
{
    reply->deleteLater();

    // Discard stale replies from a previous route
    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "RouteWeatherManager: Fetch failed:" << reply->errorString();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());

    // Multi-coordinate response is an array; single coordinate is an object
    QJsonArray results;
    if (doc.isArray()) {
        results = doc.array();
    } else if (doc.isObject()) {
        results.append(doc.object());
    }

    if (results.size() != m_points.size()) {
        qWarning() << "RouteWeatherManager: Response count mismatch:"
                   << results.size() << "vs" << m_points.size() << "points";
        // Use whatever we got
    }

    QDateTime now = QDateTime::currentDateTimeUtc();

    for (int i = 0; i < qMin(results.size(), static_cast<qsizetype>(m_points.size())); ++i) {
        QJsonObject pointData = results[i].toObject();
        QJsonObject minutely = pointData["minutely_15"].toObject();

        QJsonArray times = minutely["time"].toArray();
        QJsonArray precip = minutely["precipitation"].toArray();
        QJsonArray codes = minutely["weather_code"].toArray();
        QJsonArray temps = minutely["temperature_2m"].toArray();
        QJsonArray winds = minutely["wind_speed_10m"].toArray();

        if (times.isEmpty()) continue;

        // Find the 15-min slot closest to this point's ETA
        QDateTime targetTime = now.addSecs(qRound(m_points[i].etaMinutes * 60));
        int bestIdx = 0;
        qint64 bestDiff = INT64_MAX;

        for (int j = 0; j < times.size(); ++j) {
            QDateTime slotTime = QDateTime::fromString(times[j].toString(), Qt::ISODate);
            qint64 diff = qAbs(targetTime.secsTo(slotTime));
            if (diff < bestDiff) {
                bestDiff = diff;
                bestIdx = j;
            }
        }

        auto &pt = m_points[i];
        if (bestIdx < precip.size()) pt.precipMm = precip[bestIdx].toDouble();
        if (bestIdx < codes.size()) pt.weatherCode = codes[bestIdx].toInt();
        if (bestIdx < temps.size()) pt.tempC = temps[bestIdx].toDouble();
        if (bestIdx < winds.size()) pt.windSpeed = winds[bestIdx].toDouble();
        pt.weatherDesc = descriptionForCode(pt.weatherCode);
    }

    buildSummary();
}

void RouteWeatherManager::buildSummary()
{
    QString summary;
    QStringList alertParts;

    for (const auto &pt : m_points) {
        summary += QString("- %1: %2, %3°C")
            .arg(pt.locationLabel, pt.weatherDesc)
            .arg(pt.tempC, 0, 'f', 0);
        if (pt.precipMm > 0.0) {
            summary += QString(", %1mm precip").arg(pt.precipMm, 0, 'f', 1);
        }
        if (pt.windSpeed > 50.0) {
            summary += QString(", wind %1 km/h").arg(pt.windSpeed, 0, 'f', 0);
        }
        summary += "\n";

        // Collect severe weather for alert
        if (isSevereWeather(pt.weatherCode)) {
            alertParts.append(QString("%1 %2").arg(pt.weatherDesc, pt.locationLabel));
        }
        if (pt.windSpeed > 80.0) {
            alertParts.append(QString("Strong winds of %1 km/h %2")
                .arg(qRound(pt.windSpeed)).arg(pt.locationLabel));
        }
    }

    m_summary = summary;
    emit summaryChanged();

    if (m_context) {
        m_context->setRouteWeatherSummary(summary);
    }

    // Always emit a weather alert so Jarvis includes weather in the route briefing.
    // If there are severe conditions, lead with those. Otherwise just summarize.
    if (!alertParts.isEmpty()) {
        emit alertDetected("Route weather: " + alertParts.join(". ") + ". Full conditions: " + summary.simplified());
    } else {
        // Build a brief all-clear summary from the sampled points
        emit alertDetected("Route weather along the next 2 hours: " + summary.simplified());
    }

    qDebug() << "RouteWeatherManager: Summary updated," << m_points.size() << "points,"
             << alertParts.size() << "severe conditions";
}

void RouteWeatherManager::refreshForecasts()
{
    if (!m_active || m_routeCoordinates.isEmpty()) return;

    // Re-sample points (driver has moved, ETAs have shifted)
    sampleRoutePoints(m_routeCoordinates, m_totalDurationSec);
    fetchWeather();

    qDebug() << "RouteWeatherManager: Refreshing forecasts (" << m_points.size() << "points)";
}

QString RouteWeatherManager::descriptionForCode(int code) const
{
    switch (code) {
        case 0: return "Clear sky";
        case 1: return "Mainly clear";
        case 2: return "Partly cloudy";
        case 3: return "Overcast";
        case 45: case 48: return "Foggy";
        case 51: return "Light drizzle";
        case 53: return "Moderate drizzle";
        case 55: return "Dense drizzle";
        case 56: case 57: return "Freezing drizzle";
        case 61: return "Light rain";
        case 63: return "Moderate rain";
        case 65: return "Heavy rain";
        case 66: case 67: return "Freezing rain";
        case 71: return "Light snow";
        case 73: return "Moderate snow";
        case 75: return "Heavy snow";
        case 77: return "Snow grains";
        case 80: return "Light showers";
        case 81: return "Moderate showers";
        case 82: return "Violent showers";
        case 85: return "Light snow showers";
        case 86: return "Heavy snow showers";
        case 95: return "Thunderstorm";
        case 96: case 99: return "Thunderstorm with hail";
        default: return "Unknown";
    }
}

bool RouteWeatherManager::isSevereWeather(int code) const
{
    return code == 65 || code == 66 || code == 67
        || code == 56 || code == 57
        || code >= 73;
}

