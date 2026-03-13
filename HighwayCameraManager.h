#ifndef HIGHWAYCAMERAMANAGER_H
#define HIGHWAYCAMERAMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class HighwayCameraManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QJsonArray cameras READ cameras NOTIFY camerasChanged)
    Q_PROPERTY(int cameraCount READ cameraCount NOTIFY camerasChanged)

public:
    explicit HighwayCameraManager(QObject *parent = nullptr);

    bool active() const { return m_active; }
    QJsonArray cameras() const { return m_camerasJson; }
    int cameraCount() const { return m_camerasJson.size(); }

public slots:
    void setRouteCoordinates(const QJsonArray &coordinates, double durationSec);
    void clearRoute();

signals:
    void activeChanged();
    void camerasChanged();

private slots:
    void fetchCameras();
    void onAlbertaReply(QNetworkReply *reply);
    void onDriveBCReply(QNetworkReply *reply);

private:
    struct Camera {
        QString id;
        QString name;
        QString imageUrl;
        double lat = 0.0;
        double lon = 0.0;
        QString direction;
        QString roadName;
        QString source;
    };

    struct RoutePoint { double lat = 0.0; double lon = 0.0; };

    void processResults();
    void sampleRoutePoints(const QJsonArray &coordinates);
    bool isNearRoute(double lat, double lon, double radiusKm) const;

    QNetworkAccessManager *m_albertaNetwork;
    QNetworkAccessManager *m_drivebcNetwork;
    QTimer *m_refreshTimer;

    bool m_active = false;
    QJsonArray m_camerasJson;  // For QML consumption

    QList<Camera> m_allCameras;
    QList<Camera> m_routeCameras;
    QList<RoutePoint> m_routePoints;
    int m_pendingRequests = 0;
    int m_generation = 0;
};

#endif // HIGHWAYCAMERAMANAGER_H
