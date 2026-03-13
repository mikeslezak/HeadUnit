#ifndef ROADSURFACEMANAGER_H
#define ROADSURFACEMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPair>

class ContextAggregator;

class RoadSurfaceManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)

public:
    explicit RoadSurfaceManager(QObject *parent = nullptr);

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
    void fetchConditions();
    void onAlbertaReply(QNetworkReply *reply);
    void onDriveBCReply(QNetworkReply *reply);

private:
    struct SurfaceReport {
        QString roadName;
        QString condition;      // "Bare Dry", "Bare Wet", "Covered Snow", "Ice", etc.
        double pavementTempC;   // NaN if unavailable
        double lat;
        double lon;
        QString source;         // "511AB" or "DriveBC"
    };

    struct RoutePoint { double lat; double lon; };

    void processResults();
    void buildSummary();
    void sampleRoutePoints(const QJsonArray &coordinates);
    bool isNearRoute(double lat, double lon, double radiusKm) const;
    double haversineKm(double lat1, double lon1, double lat2, double lon2) const;

    // Decode Google Encoded Polyline to get first coordinate
    static QPair<double, double> decodePolylineFirstPoint(const QString &encoded);

    QNetworkAccessManager *m_albertaNetwork;
    QNetworkAccessManager *m_drivebcNetwork;
    QTimer *m_refreshTimer;
    ContextAggregator *m_context = nullptr;

    bool m_active = false;
    QString m_summary;

    QList<SurfaceReport> m_allReports;
    QList<SurfaceReport> m_routeReports;
    QList<RoutePoint> m_routePoints;
    QJsonArray m_routeCoordinates;
    int m_pendingRequests = 0;
};

#endif // ROADSURFACEMANAGER_H
