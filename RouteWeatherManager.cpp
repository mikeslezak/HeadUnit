#include "RouteWeatherManager.h"
#include "ContextAggregator.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QtMath>

RouteWeatherManager::RouteWeatherManager(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    connect(m_network, &QNetworkAccessManager::finished,
            this, &RouteWeatherManager::onWeatherReply);

    // Refresh route weather every 15 minutes
    m_refreshTimer->setInterval(15 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &RouteWeatherManager::refreshForecasts);

    qDebug() << "RouteWeatherManager: Initialized";
}

void RouteWeatherManager::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }

void RouteWeatherManager::setRouteCoordinates(const QJsonArray &coordinates, double durationSec)
{
    if (coordinates.size() < 2) {
        clearRoute();
        return;
    }

    m_totalDurationSec = durationSec;
    sampleRoutePoints(coordinates, durationSec);

    m_active = true;
    emit activeChanged();

    // Fetch weather for each sampled point
    m_pendingRequests = m_points.size();
    for (int i = 0; i < m_points.size(); ++i) {
        fetchPointWeather(i);
    }

    m_refreshTimer->start();
    qDebug() << "RouteWeatherManager: Tracking" << m_points.size() << "points along route";
}

void RouteWeatherManager::clearRoute()
{
    m_active = false;
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
    // Sample 6 evenly-spaced points along the route (including start and end)
    int numSamples = qMin(6, numCoords);
    if (numSamples < 2) numSamples = 2;

    for (int i = 0; i < numSamples; ++i) {
        double fraction = (numSamples == 1) ? 0.0 : static_cast<double>(i) / (numSamples - 1);
        int idx = qMin(static_cast<int>(fraction * (numCoords - 1)), numCoords - 1);

        QJsonArray coord = coordinates[idx].toArray();
        if (coord.size() < 2) continue;

        RoutePoint pt;
        pt.lon = coord[0].toDouble();
        pt.lat = coord[1].toDouble();
        pt.etaHours = (durationSec * fraction) / 3600.0;

        if (i == 0) pt.locationLabel = "Start";
        else if (i == numSamples - 1) pt.locationLabel = "Destination";
        else pt.locationLabel = QString("%1h away").arg(pt.etaHours, 0, 'f', 1);

        m_points.append(pt);
    }
}

void RouteWeatherManager::fetchPointWeather(int index)
{
    if (index < 0 || index >= m_points.size()) return;

    const auto &pt = m_points[index];

    // Open-Meteo forecast (free, no key) — get current + hourly for the next 24h
    QString url = "https://api.open-meteo.com/v1/forecast";
    QUrlQuery params;
    params.addQueryItem("latitude", QString::number(pt.lat, 'f', 4));
    params.addQueryItem("longitude", QString::number(pt.lon, 'f', 4));
    params.addQueryItem("current", "temperature_2m,weather_code,wind_speed_10m,precipitation");
    params.addQueryItem("hourly", "temperature_2m,weather_code,precipitation_probability,precipitation,wind_speed_10m");
    params.addQueryItem("forecast_hours", "24");
    params.addQueryItem("timezone", "auto");

    QUrl requestUrl(url);
    requestUrl.setQuery(params);

    QNetworkRequest req(requestUrl);
    req.setAttribute(QNetworkRequest::User, index);
    m_network->get(req);
}

void RouteWeatherManager::onWeatherReply(QNetworkReply *reply)
{
    reply->deleteLater();

    int idx = reply->request().attribute(QNetworkRequest::User).toInt();
    if (idx < 0 || idx >= m_points.size()) return;

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "RouteWeatherManager: Weather fetch failed for point" << idx << reply->errorString();
        m_pendingRequests--;
        if (m_pendingRequests <= 0) buildSummary();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();

    // Use current weather for start point, or hourly forecast at ETA for later points
    auto &pt = m_points[idx];

    if (idx == 0 || pt.etaHours < 0.5) {
        // Use current weather
        QJsonObject current = root["current"].toObject();
        pt.tempC = current["temperature_2m"].toDouble();
        pt.weatherCode = current["weather_code"].toInt();
        pt.precipMm = current["precipitation"].toDouble();
        pt.windSpeed = current["wind_speed_10m"].toDouble();
    } else {
        // Use hourly forecast at the ETA hour
        QJsonObject hourly = root["hourly"].toObject();
        QJsonArray temps = hourly["temperature_2m"].toArray();
        QJsonArray codes = hourly["weather_code"].toArray();
        QJsonArray precip = hourly["precipitation"].toArray();
        QJsonArray precipProb = hourly["precipitation_probability"].toArray();
        QJsonArray wind = hourly["wind_speed_10m"].toArray();
        int hourIdx = qMin(static_cast<int>(qRound(pt.etaHours)), temps.size() - 1);
        if (hourIdx >= 0 && hourIdx < temps.size()) {
            pt.tempC = temps[hourIdx].toDouble();
            pt.weatherCode = codes[hourIdx].toInt();
            if (hourIdx < precip.size()) pt.precipMm = precip[hourIdx].toDouble();
            if (hourIdx < precipProb.size()) pt.precipProbability = precipProb[hourIdx].toInt();
            if (hourIdx < wind.size()) pt.windSpeed = wind[hourIdx].toDouble();
        }
    }

    pt.weatherDesc = descriptionForCode(pt.weatherCode);

    m_pendingRequests--;
    if (m_pendingRequests <= 0) {
        buildSummary();
    }
}

void RouteWeatherManager::buildSummary()
{
    QString summary;
    bool alertSent = false;

    for (const auto &pt : m_points) {
        summary += QString("- %1: %2, %3°C")
            .arg(pt.locationLabel, pt.weatherDesc)
            .arg(pt.tempC, 0, 'f', 0);
        if (pt.precipMm > 0.0) {
            summary += QString(", %1mm/h precip").arg(pt.precipMm, 0, 'f', 1);
        }
        if (pt.windSpeed > 50.0) {
            summary += QString(", wind %1 km/h").arg(pt.windSpeed, 0, 'f', 0);
        }
        summary += "\n";

        // Generate conversational alerts for severe conditions
        if (!alertSent && isSevereWeather(pt.weatherCode)) {
            QString alert;
            QString timeRef = pt.locationLabel;
            if (pt.etaHours < 0.5) {
                timeRef = "at your current location";
            } else if (pt.etaHours < 1.5) {
                timeRef = "in about an hour ahead";
            } else {
                timeRef = QString("in about %1 hours ahead").arg(qRound(pt.etaHours));
            }

            if (pt.weatherCode >= 95) {
                alert = QString("Heads up, there's a thunderstorm %1 on your route").arg(timeRef);
            } else if (pt.weatherCode >= 71 && pt.weatherCode <= 77) {
                alert = QString("Watch out, there's snow %1 on your route").arg(timeRef);
            } else if (pt.weatherCode == 66 || pt.weatherCode == 67) {
                alert = QString("Careful, there's freezing rain %1 on your route. Roads could be icy").arg(timeRef);
            } else if (pt.weatherCode == 65 || pt.weatherCode == 82) {
                alert = QString("Heavy rain %1 on your route. Visibility may be reduced").arg(timeRef);
            } else if (pt.precipMm > 5.0) {
                alert = QString("Significant precipitation %1, about %2 millimeters per hour")
                    .arg(timeRef).arg(pt.precipMm, 0, 'f', 1);
            } else {
                alert = QString("Weather alert: %1 expected %2 on your route")
                    .arg(pt.weatherDesc, timeRef);
            }

            emit alertDetected(alert);
            alertSent = true;
        }

        // High wind alert (separate from precipitation)
        if (!alertSent && pt.windSpeed > 80.0) {
            QString timeRef = pt.etaHours < 0.5 ? "right now" :
                QString("in about %1 hours").arg(qRound(pt.etaHours));
            emit alertDetected(QString("Strong winds of %1 kilometers per hour %2 on your route")
                .arg(qRound(pt.windSpeed)).arg(timeRef));
            alertSent = true;
        }
    }

    m_summary = summary;
    emit summaryChanged();

    if (m_context) {
        m_context->setRouteWeatherSummary(summary);
    }

    qDebug() << "RouteWeatherManager: Summary updated," << m_points.size() << "points";
}

void RouteWeatherManager::refreshForecasts()
{
    if (!m_active || m_points.isEmpty()) return;
    qDebug() << "RouteWeatherManager: Refreshing forecasts";
    m_pendingRequests = m_points.size();
    for (int i = 0; i < m_points.size(); ++i) {
        fetchPointWeather(i);
    }
}

QString RouteWeatherManager::descriptionForCode(int code) const
{
    // WMO weather interpretation codes
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
    // Heavy rain, freezing rain/drizzle, moderate+ snow, thunderstorms
    return code == 65 || code == 66 || code == 67
        || code == 56 || code == 57
        || code >= 73;
}
