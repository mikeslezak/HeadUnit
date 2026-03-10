#include "WeatherManager.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

WeatherManager::WeatherManager(QObject *parent)
    : QObject(parent)
    , m_locationNetwork(new QNetworkAccessManager(this))
    , m_weatherNetwork(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    connect(m_locationNetwork, &QNetworkAccessManager::finished,
            this, &WeatherManager::onLocationReply);
    connect(m_weatherNetwork, &QNetworkAccessManager::finished,
            this, &WeatherManager::onWeatherReply);

    // Auto-refresh every 15 minutes
    m_refreshTimer->setInterval(15 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &WeatherManager::refresh);
    m_refreshTimer->start();

    // Initial fetch
    QTimer::singleShot(500, this, &WeatherManager::fetchLocation);
}

void WeatherManager::refresh()
{
    if (m_hasLocation) {
        fetchWeather();
    } else {
        fetchLocation();
    }
}

void WeatherManager::setLocation(double lat, double lon)
{
    m_latitude = lat;
    m_longitude = lon;
    m_hasLocation = true;
    fetchWeather();
}

void WeatherManager::fetchLocation()
{
    m_loading = true;
    emit loadingChanged();

    // Use ip-api.com for geolocation (free, no key needed)
    QNetworkRequest request(QUrl("http://ip-api.com/json/?fields=lat,lon,city,regionName"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "HeadUnit/1.0");
    m_locationNetwork->get(request);
}

void WeatherManager::onLocationReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Location fetch failed:" << reply->errorString();
        m_errorMessage = "Could not determine location";
        m_loading = false;
        emit loadingChanged();
        emit errorOccurred(m_errorMessage);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();

    m_latitude = obj["lat"].toDouble();
    m_longitude = obj["lon"].toDouble();
    m_locationName = obj["city"].toString();
    QString region = obj["regionName"].toString();
    if (!region.isEmpty() && !m_locationName.isEmpty()) {
        m_locationName += ", " + region;
    }
    m_hasLocation = true;

    qDebug() << "Weather location:" << m_locationName << m_latitude << m_longitude;
    emit locationUpdated();

    fetchWeather();
}

void WeatherManager::fetchWeather()
{
    if (!m_hasLocation) {
        fetchLocation();
        return;
    }

    m_loading = true;
    emit loadingChanged();

    // Open-Meteo API (free, no key needed)
    QUrl url("https://api.open-meteo.com/v1/forecast");
    QUrlQuery query;
    query.addQueryItem("latitude", QString::number(m_latitude, 'f', 4));
    query.addQueryItem("longitude", QString::number(m_longitude, 'f', 4));
    query.addQueryItem("current", "temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,wind_direction_10m");
    query.addQueryItem("hourly", "temperature_2m,weather_code,is_day");
    query.addQueryItem("daily", "weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset");
    query.addQueryItem("temperature_unit", "celsius");
    query.addQueryItem("wind_speed_unit", "kmh");
    query.addQueryItem("forecast_days", "7");
    query.addQueryItem("timezone", "auto");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "HeadUnit/1.0");
    m_weatherNetwork->get(request);
}

void WeatherManager::onWeatherReply(QNetworkReply *reply)
{
    reply->deleteLater();

    m_loading = false;
    emit loadingChanged();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Weather fetch failed:" << reply->errorString();
        m_errorMessage = "Weather update failed";
        emit errorOccurred(m_errorMessage);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();

    // Parse current weather
    QJsonObject current = root["current"].toObject();
    m_temperature = current["temperature_2m"].toDouble();
    m_feelsLike = current["apparent_temperature"].toDouble();
    m_humidity = current["relative_humidity_2m"].toInt();
    m_windSpeed = current["wind_speed_10m"].toDouble();
    m_windDirection = current["wind_direction_10m"].toInt();
    m_weatherCode = current["weather_code"].toInt();
    m_isDay = current["is_day"].toInt() == 1;
    m_weatherDescription = descriptionForCode(m_weatherCode);
    m_weatherIcon = iconForCode(m_weatherCode, m_isDay);

    // Parse hourly forecast (next 24 hours)
    QJsonObject hourly = root["hourly"].toObject();
    QJsonArray hourlyTimes = hourly["time"].toArray();
    QJsonArray hourlyTemps = hourly["temperature_2m"].toArray();
    QJsonArray hourlyCodes = hourly["weather_code"].toArray();
    QJsonArray hourlyIsDay = hourly["is_day"].toArray();

    QJsonArray hourlyArr;
    QDateTime now = QDateTime::currentDateTime();
    int startIdx = -1;

    // Find current hour index
    for (int i = 0; i < hourlyTimes.size(); ++i) {
        QDateTime t = QDateTime::fromString(hourlyTimes[i].toString(), Qt::ISODate);
        if (t >= now) {
            startIdx = i;
            break;
        }
    }

    if (startIdx >= 0) {
        for (int i = startIdx; i < qMin(startIdx + 24, hourlyTimes.size()); ++i) {
            QJsonObject h;
            QDateTime t = QDateTime::fromString(hourlyTimes[i].toString(), Qt::ISODate);
            h["time"] = t.toString("ha");
            h["temp"] = qRound(hourlyTemps[i].toDouble());
            h["icon"] = iconForCode(hourlyCodes[i].toInt(), hourlyIsDay[i].toInt() == 1);
            hourlyArr.append(h);
        }
    }
    m_hourlyForecast = hourlyArr;

    // Parse daily forecast
    QJsonObject daily = root["daily"].toObject();
    QJsonArray dailyTimes = daily["time"].toArray();
    QJsonArray dailyMaxTemps = daily["temperature_2m_max"].toArray();
    QJsonArray dailyMinTemps = daily["temperature_2m_min"].toArray();
    QJsonArray dailyCodes = daily["weather_code"].toArray();

    QJsonArray dailyArr;
    QStringList dayNames = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (int i = 0; i < dailyTimes.size(); ++i) {
        QJsonObject d;
        QDate date = QDate::fromString(dailyTimes[i].toString(), "yyyy-MM-dd");
        d["day"] = (i == 0) ? QString("Today") : dayNames[date.dayOfWeek() % 7];
        d["high"] = qRound(dailyMaxTemps[i].toDouble());
        d["low"] = qRound(dailyMinTemps[i].toDouble());
        d["icon"] = iconForCode(dailyCodes[i].toInt(), true);
        d["code"] = dailyCodes[i].toInt();
        dailyArr.append(d);
    }
    m_dailyForecast = dailyArr;

    m_lastUpdated = QDateTime::currentDateTime().toString("h:mm AP");
    m_errorMessage.clear();

    qDebug() << "Weather updated:" << m_temperature << "°C," << m_weatherDescription;
    emit weatherUpdated();
}

QString WeatherManager::descriptionForCode(int code) const
{
    switch (code) {
        case 0: return "Clear Sky";
        case 1: return "Mainly Clear";
        case 2: return "Partly Cloudy";
        case 3: return "Overcast";
        case 45: case 48: return "Foggy";
        case 51: return "Light Drizzle";
        case 53: return "Drizzle";
        case 55: return "Heavy Drizzle";
        case 56: case 57: return "Freezing Drizzle";
        case 61: return "Light Rain";
        case 63: return "Rain";
        case 65: return "Heavy Rain";
        case 66: case 67: return "Freezing Rain";
        case 71: return "Light Snow";
        case 73: return "Snow";
        case 75: return "Heavy Snow";
        case 77: return "Snow Grains";
        case 80: return "Light Showers";
        case 81: return "Showers";
        case 82: return "Heavy Showers";
        case 85: return "Light Snow Showers";
        case 86: return "Snow Showers";
        case 95: return "Thunderstorm";
        case 96: case 99: return "Thunderstorm with Hail";
        default: return "Unknown";
    }
}

QString WeatherManager::iconForCode(int code, bool isDay) const
{
    switch (code) {
        case 0: return isDay ? "☀️" : "🌙";
        case 1: return isDay ? "🌤️" : "🌙";
        case 2: return isDay ? "⛅" : "☁️";
        case 3: return "☁️";
        case 45: case 48: return "🌫️";
        case 51: case 53: case 55: case 56: case 57: return "🌧️";
        case 61: case 63: case 80: case 81: return "🌧️";
        case 65: case 82: return "🌧️";
        case 66: case 67: return "🌨️";
        case 71: case 73: case 77: case 85: return "🌨️";
        case 75: case 86: return "❄️";
        case 95: case 96: case 99: return "⛈️";
        default: return "🌡️";
    }
}
