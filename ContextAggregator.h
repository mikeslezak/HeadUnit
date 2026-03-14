#ifndef CONTEXTAGGREGATOR_H
#define CONTEXTAGGREGATOR_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QPair>
#include <QList>

class WeatherManager;
class VehicleBusManager;
class TidalClient;
class SpotifyClient;
class MediaController;

class ContextAggregator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double gpsLatitude READ gpsLatitude WRITE setGpsLatitude NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLongitude READ gpsLongitude WRITE setGpsLongitude NOTIFY gpsChanged)
    Q_PROPERTY(double gpsSpeed READ gpsSpeed WRITE setGpsSpeed NOTIFY gpsChanged)
    Q_PROPERTY(double gpsHeading READ gpsHeading WRITE setGpsHeading NOTIFY gpsChanged)
    Q_PROPERTY(bool routeActive READ routeActive WRITE setRouteActive NOTIFY routeChanged)
    Q_PROPERTY(QString routeDestination READ routeDestination WRITE setRouteDestination NOTIFY routeChanged)
    Q_PROPERTY(QString routeDistance READ routeDistance WRITE setRouteDistance NOTIFY routeChanged)
    Q_PROPERTY(QString routeDuration READ routeDuration WRITE setRouteDuration NOTIFY routeChanged)

public:
    explicit ContextAggregator(QObject *parent = nullptr);

    void setWeatherManager(WeatherManager *mgr);
    void setVehicleBusManager(VehicleBusManager *mgr);
    void setTidalClient(TidalClient *client);
    void setSpotifyClient(SpotifyClient *client);
    void setMediaController(MediaController *mgr);

    double gpsLatitude() const { return m_gpsLat; }
    double gpsLongitude() const { return m_gpsLon; }

    // Best available location (real GPS, or WeatherManager ip-api fallback)
    double bestLatitude() const;
    double bestLongitude() const;
    double gpsSpeed() const { return m_gpsSpeed; }
    double gpsHeading() const { return m_gpsHeading; }
    bool routeActive() const { return m_routeActive; }
    QString routeDestination() const { return m_routeDest; }
    QString routeDistance() const { return m_routeDist; }
    QString routeDuration() const { return m_routeDur; }

    void setGpsLatitude(double lat);
    void setGpsLongitude(double lon);
    void setGpsSpeed(double speed);
    void setGpsHeading(double heading);
    void setRouteActive(bool active);
    void setRouteDestination(const QString &dest);
    void setRouteDistance(const QString &dist);
    void setRouteDuration(const QString &dur);

    Q_INVOKABLE QString buildContext() const;

    // Route weather summary (set by RouteWeatherManager)
    void setRouteWeatherSummary(const QString &summary);

    // Road conditions summary (set by RoadConditionManager)
    void setRoadConditionsSummary(const QString &summary);

    // Speed limit summary (set by SpeedLimitManager)
    void setSpeedLimitSummary(const QString &summary);

    // Road surface conditions (set by RoadSurfaceManager)
    void setRoadSurfaceSummary(const QString &summary);

    // Avalanche conditions (set by AvalancheManager)
    void setAvalancheSummary(const QString &summary);

    // Border wait times (set by BorderWaitManager)
    void setBorderWaitSummary(const QString &summary);

    // Route sample points for along-route searches (lat/lon pairs evenly spaced)
    Q_INVOKABLE void setRouteCoordinates(const QJsonArray &coordinates, double durationSec);
    Q_INVOKABLE void clearRouteCoordinates();
    QList<QPair<double,double>> routeSamplePoints(int count = 3) const;
    const QJsonArray &routeCoordinates() const { return m_routeCoords; }

signals:
    void gpsChanged();
    void routeChanged();

private:
    WeatherManager *m_weather = nullptr;
    VehicleBusManager *m_vehicle = nullptr;
    TidalClient *m_tidal = nullptr;
    SpotifyClient *m_spotify = nullptr;
    MediaController *m_media = nullptr;

    double m_gpsLat = 0.0;
    double m_gpsLon = 0.0;
    double m_gpsSpeed = 0.0;
    double m_gpsHeading = 0.0;

    bool m_routeActive = false;
    QString m_routeDest;
    QString m_routeDist;
    QString m_routeDur;
    QString m_routeWeatherSummary;
    QString m_roadConditionsSummary;
    QString m_speedLimitSummary;
    QString m_roadSurfaceSummary;
    QString m_avalancheSummary;
    QString m_borderWaitSummary;
    QJsonArray m_routeCoords;
};

#endif // CONTEXTAGGREGATOR_H
