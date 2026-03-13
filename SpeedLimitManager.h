#ifndef SPEEDLIMITMANAGER_H
#define SPEEDLIMITMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QElapsedTimer>

class ContextAggregator;

class SpeedLimitManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int currentSpeedLimit READ currentSpeedLimit NOTIFY currentSpeedLimitChanged)
    Q_PROPERTY(bool speeding READ speeding NOTIFY speedingChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)

public:
    explicit SpeedLimitManager(QObject *parent = nullptr);

    void setContextAggregator(ContextAggregator *ctx);

    bool active() const { return m_active; }
    int currentSpeedLimit() const { return m_currentSpeedLimit; }
    bool speeding() const { return m_speeding; }
    QString summary() const { return m_summary; }

public slots:
    // Called from Maps.qml with Mapbox maxspeed annotation data + route geometry
    void setSpeedLimitData(const QJsonArray &maxspeedAnnotations, const QJsonArray &routeCoordinates, double durationSec);
    // Called from Maps.qml onPositionChanged
    void updateGpsPosition(double lat, double lon, double speedKmh);
    void clearRoute();

signals:
    void activeChanged();
    void currentSpeedLimitChanged();
    void speedingChanged();
    void summaryChanged();
    void alertDetected(const QString &message);

private:
    struct SpeedSegment {
        double lat;
        double lon;
        int speedKmh;  // 0 if unknown
        bool isNone;    // true if maxspeed was "none" (no limit)
    };

    void buildSummary();
    double haversineKm(double lat1, double lon1, double lat2, double lon2) const;

    ContextAggregator *m_context = nullptr;
    bool m_active = false;
    int m_currentSpeedLimit = 0;
    bool m_speeding = false;
    QString m_summary;

    QList<SpeedSegment> m_segments;
    int m_lastSegmentIndex = 0; // sequential tracking - only search forward

    // Speeding alert: only alert after sustained speeding (10+ seconds)
    bool m_speedingStarted = false;
    QElapsedTimer m_speedingTimer;
    bool m_speedingAlertSent = false;
};

#endif // SPEEDLIMITMANAGER_H
