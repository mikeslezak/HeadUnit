#ifndef ROUTEWEATHERMANAGER_H
#define ROUTEWEATHERMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ContextAggregator;

class RouteWeatherManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)

public:
    explicit RouteWeatherManager(QObject *parent = nullptr);

    void setContextAggregator(ContextAggregator *ctx);

    bool active() const { return m_active; }
    QString summary() const { return m_summary; }

public slots:
    void setRouteCoordinates(const QJsonArray &coordinates, double durationSec);
    void clearRoute();

signals:
    void activeChanged();
    void summaryChanged();
    void alertDetected(const QString &message);

private slots:
    void onWeatherReply(QNetworkReply *reply);
    void refreshForecasts();

private:
    struct RoutePoint {
        double lat = 0.0;
        double lon = 0.0;
        double etaMinutes = 0.0; // minutes from now to reach this point
        QString weatherDesc;
        double tempC = 0.0;
        int weatherCode = 0;
        double precipMm = 0.0;   // precipitation mm
        double windSpeed = 0.0;  // km/h
        QString locationLabel;
    };

    void sampleRoutePoints(const QJsonArray &coordinates, double durationSec);
    void fetchWeather();
    void buildSummary();
    QString descriptionForCode(int code) const;
    bool isSevereWeather(int code) const;
    QNetworkAccessManager *m_network;
    QTimer *m_refreshTimer;
    ContextAggregator *m_context = nullptr;

    bool m_active = false;
    QString m_summary;
    QList<RoutePoint> m_points;
    QJsonArray m_routeCoordinates;
    double m_totalDurationSec = 0.0;
    int m_generation = 0;

    static constexpr double SAMPLE_INTERVAL_KM = 15.0;   // sample every 15km
    static constexpr double LOOKAHEAD_HOURS = 2.0;        // only look 2 hours ahead
    static constexpr int REFRESH_INTERVAL_MS = 30 * 60 * 1000; // 30 minutes
};

#endif // ROUTEWEATHERMANAGER_H
