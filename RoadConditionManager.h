#ifndef ROADCONDITIONMANAGER_H
#define ROADCONDITIONMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ContextAggregator;

class RoadConditionManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)

public:
    explicit RoadConditionManager(QObject *parent = nullptr);

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
    struct RoadEvent {
        QString id;
        QString roadName;
        QString description;
        QString eventType;   // closures, construction, incident, hazard
        QString severity;    // Minor, Moderate, Major
        double lat = 0.0;
        double lon = 0.0;
        bool fullClosure = false;
    };

    void processEvents();
    void buildSummary();
    bool isOnRoute(double lat, double lon) const;
    QString shortenDescription(const QString &desc) const;

    QNetworkAccessManager *m_albertaNetwork;
    QNetworkAccessManager *m_drivebcNetwork;
    QTimer *m_refreshTimer;
    ContextAggregator *m_context = nullptr;

    bool m_active = false;
    QString m_summary;

    QList<RoadEvent> m_allEvents;
    QList<RoadEvent> m_routeEvents;  // filtered to route proximity
    QJsonArray m_routeCoordinates;
    int m_pendingRequests = 0;
    int m_generation = 0;

    // Sample points along route for proximity checks
    struct RoutePoint { double lat; double lon; };
    QList<RoutePoint> m_routePoints;
    void sampleRoutePoints(const QJsonArray &coordinates);
};

#endif // ROADCONDITIONMANAGER_H
