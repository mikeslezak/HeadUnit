#ifndef AVALANCHEMANAGER_H
#define AVALANCHEMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ContextAggregator;

class AvalancheManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(QString highestDanger READ highestDanger NOTIFY summaryChanged)

public:
    explicit AvalancheManager(QObject *parent = nullptr);

    void setContextAggregator(ContextAggregator *ctx);

    bool active() const { return m_active; }
    QString summary() const { return m_summary; }
    QString highestDanger() const { return m_highestDanger; }

public slots:
    void setRouteCoordinates(const QJsonArray &coordinates, double durationSec, bool silent = false);
    void clearRoute();

signals:
    void activeChanged();
    void summaryChanged();
    void alertDetected(const QString &message);

private slots:
    void onForecastReply(QNetworkReply *reply);
    void refreshForecasts();

private:
    struct ForecastPoint {
        double lat = 0.0;
        double lon = 0.0;
        QString locationLabel;
        int dangerAlpine = 0;    // 1-5
        int dangerTreeline = 0;
        int dangerBelowTree = 0;
        QString highlights;
        bool hasForecast = false;
    };

    void sampleMountainPoints(const QJsonArray &coordinates, double durationSec);
    void fetchForecasts();
    void buildSummary();
    QString dangerLevelName(int level) const;

    QNetworkAccessManager *m_network;
    QTimer *m_refreshTimer;
    ContextAggregator *m_context = nullptr;

    bool m_active = false;
    QString m_summary;
    QString m_highestDanger;

    QList<ForecastPoint> m_points;
    int m_pendingRequests = 0;
    int m_generation = 0;

    // Change detection — only emit alertDetected when danger level changes
    int m_lastHighestDanger = 0;
    bool m_suppressNextAlert = false;
};

#endif // AVALANCHEMANAGER_H
