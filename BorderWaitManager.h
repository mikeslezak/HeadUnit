#ifndef BORDERWAITMANAGER_H
#define BORDERWAITMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ContextAggregator;

class BorderWaitManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(QString nearestCrossing READ nearestCrossing NOTIFY summaryChanged)
    Q_PROPERTY(int waitMinutes READ waitMinutes NOTIFY summaryChanged)

public:
    explicit BorderWaitManager(QObject *parent = nullptr);

    void setContextAggregator(ContextAggregator *ctx);

    bool active() const { return m_active; }
    QString summary() const { return m_summary; }
    QString nearestCrossing() const { return m_nearestCrossing; }
    int waitMinutes() const { return m_waitMinutes; }

public slots:
    void setRouteCoordinates(const QJsonArray &coordinates, double durationSec);
    void clearRoute();

signals:
    void activeChanged();
    void summaryChanged();
    void alertDetected(const QString &message);

private slots:
    void fetchWaitTimes();
    void onCbpReply(QNetworkReply *reply);
    void onCbsaReply(QNetworkReply *reply);

private:
    struct KnownCrossing {
        QString name;
        double lat;
        double lon;
        int cbpPortNumber;  // for matching CBP data, 0 if CBSA-only
    };

    struct WaitTimeData {
        QString crossingName;
        int commercialMinutes = -1;  // -1 = unknown
        int passengerMinutes = -1;
        int lanesOpen = 0;
        QString lastUpdated;
        double lat;
        double lon;
        QString source;
    };

    struct RoutePoint { double lat; double lon; };

    void processResults();
    void buildSummary();
    void sampleRoutePoints(const QJsonArray &coordinates);
    bool isNearBorder() const;

    static QList<KnownCrossing> knownCrossings();

    QNetworkAccessManager *m_cbpNetwork;
    QNetworkAccessManager *m_cbsaNetwork;
    QTimer *m_refreshTimer;
    ContextAggregator *m_context = nullptr;

    bool m_active = false;
    QString m_summary;
    QString m_nearestCrossing;
    int m_waitMinutes = -1;

    QList<WaitTimeData> m_waitData;
    QList<RoutePoint> m_routePoints;
    QJsonArray m_routeCoordinates;
    int m_pendingRequests = 0;
    int m_generation = 0;
};

#endif // BORDERWAITMANAGER_H
