#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>

class WeatherManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double temperature READ temperature NOTIFY weatherUpdated)
    Q_PROPERTY(double feelsLike READ feelsLike NOTIFY weatherUpdated)
    Q_PROPERTY(int humidity READ humidity NOTIFY weatherUpdated)
    Q_PROPERTY(double windSpeed READ windSpeed NOTIFY weatherUpdated)
    Q_PROPERTY(int windDirection READ windDirection NOTIFY weatherUpdated)
    Q_PROPERTY(int weatherCode READ weatherCode NOTIFY weatherUpdated)
    Q_PROPERTY(QString weatherDescription READ weatherDescription NOTIFY weatherUpdated)
    Q_PROPERTY(QString weatherIcon READ weatherIcon NOTIFY weatherUpdated)
    Q_PROPERTY(bool isDay READ isDay NOTIFY weatherUpdated)
    Q_PROPERTY(QString locationName READ locationName NOTIFY locationUpdated)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorOccurred)
    Q_PROPERTY(QJsonArray hourlyForecast READ hourlyForecast NOTIFY weatherUpdated)
    Q_PROPERTY(QJsonArray dailyForecast READ dailyForecast NOTIFY weatherUpdated)
    Q_PROPERTY(QString lastUpdated READ lastUpdated NOTIFY weatherUpdated)

public:
    explicit WeatherManager(QObject *parent = nullptr);

    double temperature() const { return m_temperature; }
    double feelsLike() const { return m_feelsLike; }
    int humidity() const { return m_humidity; }
    double windSpeed() const { return m_windSpeed; }
    int windDirection() const { return m_windDirection; }
    int weatherCode() const { return m_weatherCode; }
    QString weatherDescription() const { return m_weatherDescription; }
    QString weatherIcon() const { return m_weatherIcon; }
    bool isDay() const { return m_isDay; }
    QString locationName() const { return m_locationName; }
    bool loading() const { return m_loading; }
    QString errorMessage() const { return m_errorMessage; }
    QJsonArray hourlyForecast() const { return m_hourlyForecast; }
    QJsonArray dailyForecast() const { return m_dailyForecast; }
    QString lastUpdated() const { return m_lastUpdated; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setLocation(double lat, double lon);

signals:
    void weatherUpdated();
    void locationUpdated();
    void loadingChanged();
    void errorOccurred(const QString &message);

private slots:
    void onLocationReply(QNetworkReply *reply);
    void onWeatherReply(QNetworkReply *reply);

private:
    void fetchLocation();
    void fetchWeather();
    QString descriptionForCode(int code) const;
    QString iconForCode(int code, bool isDay) const;

    QNetworkAccessManager *m_locationNetwork;
    QNetworkAccessManager *m_weatherNetwork;
    QTimer *m_refreshTimer;

    double m_latitude = 0.0;
    double m_longitude = 0.0;
    bool m_hasLocation = false;

    double m_temperature = 0.0;
    double m_feelsLike = 0.0;
    int m_humidity = 0;
    double m_windSpeed = 0.0;
    int m_windDirection = 0;
    int m_weatherCode = 0;
    QString m_weatherDescription;
    QString m_weatherIcon;
    bool m_isDay = true;
    QString m_locationName;
    bool m_loading = false;
    QString m_errorMessage;
    QJsonArray m_hourlyForecast;
    QJsonArray m_dailyForecast;
    QString m_lastUpdated;
};
