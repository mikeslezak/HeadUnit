#ifndef COPILOTMONITOR_H
#define COPILOTMONITOR_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>

class ContextAggregator;
class VehicleBusManager;
class RouteWeatherManager;
class RoadConditionManager;
class SpeedLimitManager;
class RoadSurfaceManager;
class AvalancheManager;
class BorderWaitManager;

class CopilotMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool quietMode READ quietMode WRITE setQuietMode NOTIFY quietModeChanged)

public:
    explicit CopilotMonitor(QObject *parent = nullptr);

    void setContextAggregator(ContextAggregator *ctx);
    void setVehicleBusManager(VehicleBusManager *vehicle);
    void setRouteWeatherManager(RouteWeatherManager *routeWeather);
    void setRoadConditionManager(RoadConditionManager *roadConditions);
    void setSpeedLimitManager(SpeedLimitManager *speedLimit);
    void setRoadSurfaceManager(RoadSurfaceManager *roadSurface);
    void setAvalancheManager(AvalancheManager *avalanche);
    void setBorderWaitManager(BorderWaitManager *borderWait);

    bool enabled() const { return m_enabled; }
    bool quietMode() const { return m_quietMode; }

    void setEnabled(bool on);
    Q_INVOKABLE void setQuietMode(bool quiet);

    // Call when a route is cleared to discard stale route alerts
    Q_INVOKABLE void clearPendingAlerts();

signals:
    void enabledChanged();
    void quietModeChanged();
    void proactiveAlert(const QString &message);

private slots:
    void checkConditions();
    void onRouteAlert(const QString &message);
    void onSpeedLimitAlert(const QString &message);

private:
    bool shouldThrottle(const QString &alertType);
    void emitAlert(const QString &alertType, const QString &message);
    void queueRouteAlert(const QString &message);
    void flushRouteAlerts();

    ContextAggregator *m_context = nullptr;
    VehicleBusManager *m_vehicle = nullptr;
    RouteWeatherManager *m_routeWeather = nullptr;
    RoadConditionManager *m_roadConditions = nullptr;
    SpeedLimitManager *m_speedLimit = nullptr;
    RoadSurfaceManager *m_roadSurface = nullptr;
    AvalancheManager *m_avalanche = nullptr;
    BorderWaitManager *m_borderWait = nullptr;

    QTimer *m_checkTimer;
    bool m_enabled = true;
    bool m_quietMode = false;

    // Batch route alerts: collect for 3 seconds then emit as one
    QTimer *m_routeAlertBatchTimer;
    QStringList m_pendingRouteAlerts;

    // Driving duration tracking
    QElapsedTimer m_drivingTimer;
    bool m_isDriving = false;
    bool m_drivingAlertSent = false;

    // Alert throttling: alertType -> last alert time (msec since epoch)
    QMap<QString, qint64> m_lastAlertTime;
    static const int ALERT_COOLDOWN_MS = 30 * 60 * 1000;  // 30 minutes
    static const int MIN_ALERT_INTERVAL_MS = 10 * 60 * 1000;  // 10 min between any alerts
    qint64 m_lastAnyAlertTime = 0;
};

#endif // COPILOTMONITOR_H
