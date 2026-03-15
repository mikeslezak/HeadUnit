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
    void setRouteCoordinates(const QJsonArray &coordinates, double durationSec, bool silent = false);
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
        QString condition;          // "Bare Dry", "Bare Wet", "Covered Snow", "Ice", etc.
        double pavementTempC = 0.0; // NaN if unavailable
        double lat = 0.0;
        double lon = 0.0;
        QString source;             // "511AB" or "DriveBC"
    };

    void processResults();
    void buildSummary();
    bool isNearRoute(double lat, double lon) const;
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
    QJsonArray m_routeCoordinates;
    int m_pendingRequests = 0;
    int m_generation = 0;

    // Change detection — only emit alertDetected when alert text differs
    QString m_lastAlertText;
    bool m_suppressNextAlert = false;
};

#endif // ROADSURFACEMANAGER_H
