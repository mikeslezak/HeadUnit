#include "RoadConditionManager.h"
#include "ContextAggregator.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QtMath>

RoadConditionManager::RoadConditionManager(QObject *parent)
    : QObject(parent)
    , m_albertaNetwork(new QNetworkAccessManager(this))
    , m_drivebcNetwork(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    connect(m_albertaNetwork, &QNetworkAccessManager::finished,
            this, &RoadConditionManager::onAlbertaReply);
    connect(m_drivebcNetwork, &QNetworkAccessManager::finished,
            this, &RoadConditionManager::onDriveBCReply);

    // Refresh every 5 minutes
    m_refreshTimer->setInterval(5 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &RoadConditionManager::fetchConditions);

    qDebug() << "RoadConditionManager: Initialized";
}

void RoadConditionManager::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }

void RoadConditionManager::setRouteCoordinates(const QJsonArray &coordinates, double durationSec)
{
    Q_UNUSED(durationSec)

    if (coordinates.size() < 2) {
        clearRoute();
        return;
    }

    m_routeCoordinates = coordinates;
    sampleRoutePoints(coordinates);

    m_active = true;
    emit activeChanged();

    fetchConditions();
    m_refreshTimer->start();

    qDebug() << "RoadConditionManager: Tracking route with" << m_routePoints.size() << "sample points";
}

void RoadConditionManager::clearRoute()
{
    m_active = false;
    m_routeCoordinates = QJsonArray();
    m_routePoints.clear();
    m_allEvents.clear();
    m_routeEvents.clear();
    m_summary.clear();
    m_refreshTimer->stop();
    emit activeChanged();
    emit summaryChanged();

    if (m_context) {
        m_context->setRoadConditionsSummary(QString());
    }
    qDebug() << "RoadConditionManager: Route cleared";
}

void RoadConditionManager::sampleRoutePoints(const QJsonArray &coordinates)
{
    m_routePoints.clear();
    int numCoords = coordinates.size();
    // Sample ~20 points along the route for proximity checks
    int numSamples = qMin(20, numCoords);
    if (numSamples < 2) numSamples = 2;

    for (int i = 0; i < numSamples; ++i) {
        double fraction = static_cast<double>(i) / (numSamples - 1);
        int idx = qMin(static_cast<int>(fraction * (numCoords - 1)), numCoords - 1);

        QJsonArray coord = coordinates[idx].toArray();
        if (coord.size() >= 2) {
            m_routePoints.append({ coord[1].toDouble(), coord[0].toDouble() }); // lat, lon
        }
    }
}

void RoadConditionManager::fetchConditions()
{
    m_allEvents.clear();
    m_pendingRequests = 0;

    // Build bounding box from route points or current GPS
    double minLat = 90, maxLat = -90, minLon = 180, maxLon = -180;

    if (!m_routePoints.isEmpty()) {
        for (const auto &pt : m_routePoints) {
            minLat = qMin(minLat, pt.lat);
            maxLat = qMax(maxLat, pt.lat);
            minLon = qMin(minLon, pt.lon);
            maxLon = qMax(maxLon, pt.lon);
        }
        // Add padding (roughly 50km)
        minLat -= 0.5; maxLat += 0.5;
        minLon -= 0.5; maxLon += 0.5;
    } else if (m_context && m_context->gpsLatitude() != 0.0) {
        // No route — use GPS position with 100km radius
        double lat = m_context->gpsLatitude();
        double lon = m_context->gpsLongitude();
        minLat = lat - 1.0; maxLat = lat + 1.0;
        minLon = lon - 1.0; maxLon = lon + 1.0;
    } else {
        return; // No location data
    }

    // 511 Alberta — fetch all active events
    m_pendingRequests++;
    QUrl abUrl("https://prod-ab.ibi511.com/api/v2/get/event");
    QNetworkRequest abReq(abUrl);
    abReq.setRawHeader("Accept", "application/json");
    m_albertaNetwork->get(abReq);

    // DriveBC — fetch active events with bounding box
    m_pendingRequests++;
    QString bcUrlStr = QString("https://api.open511.gov.bc.ca/events?format=json&status=ACTIVE"
        "&area_id=drivebc.ca&bbox=%1,%2,%3,%4&limit=50")
        .arg(minLon, 0, 'f', 4).arg(minLat, 0, 'f', 4)
        .arg(maxLon, 0, 'f', 4).arg(maxLat, 0, 'f', 4);
    QUrl bcUrl(bcUrlStr);
    QNetworkRequest bcReq(bcUrl);
    m_drivebcNetwork->get(bcReq);

    qDebug() << "RoadConditionManager: Fetching conditions, bbox:"
             << minLat << minLon << "to" << maxLat << maxLon;
}

void RoadConditionManager::onAlbertaReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "RoadConditionManager: Alberta fetch failed:" << reply->errorString();
        m_pendingRequests--;
        if (m_pendingRequests <= 0) processEvents();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray events = doc.array();

    for (const QJsonValue &v : events) {
        QJsonObject obj = v.toObject();
        RoadEvent ev;
        ev.id = QString("ab-%1").arg(obj["ID"].toInt());
        ev.roadName = obj["RoadwayName"].toString();
        ev.description = obj["Description"].toString();
        ev.lat = obj["Latitude"].toDouble();
        ev.lon = obj["Longitude"].toDouble();
        ev.fullClosure = obj["IsFullClosure"].toBool();

        QString type = obj["EventType"].toString().toLower();
        ev.eventType = type;

        QString severity = obj["Severity"].toString();
        if (ev.fullClosure) severity = "Major";
        ev.severity = severity;

        // Skip events with no valid location
        if (ev.lat == 0.0 && ev.lon == 0.0) continue;

        m_allEvents.append(ev);
    }

    qDebug() << "RoadConditionManager: Alberta returned" << events.size() << "events";
    m_pendingRequests--;
    if (m_pendingRequests <= 0) processEvents();
}

void RoadConditionManager::onDriveBCReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "RoadConditionManager: DriveBC fetch failed:" << reply->errorString();
        m_pendingRequests--;
        if (m_pendingRequests <= 0) processEvents();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    QJsonArray events = root["events"].toArray();

    for (const QJsonValue &v : events) {
        QJsonObject obj = v.toObject();
        RoadEvent ev;
        ev.id = obj["id"].toString();
        ev.description = obj["description"].toString();
        ev.severity = obj["severity"].toString();
        ev.eventType = obj["event_type"].toString().toLower();

        // Extract road name
        QJsonArray roads = obj["roads"].toArray();
        if (!roads.isEmpty()) {
            ev.roadName = roads[0].toObject()["name"].toString();
        }

        // Extract coordinates from geometry
        QJsonObject geo = obj["geography"].toObject();
        if (geo["type"].toString() == "Point") {
            QJsonArray coords = geo["coordinates"].toArray();
            if (coords.size() >= 2) {
                ev.lon = coords[0].toDouble();
                ev.lat = coords[1].toDouble();
            }
        }

        ev.fullClosure = ev.description.contains("closed", Qt::CaseInsensitive);

        if (ev.lat == 0.0 && ev.lon == 0.0) continue;

        m_allEvents.append(ev);
    }

    qDebug() << "RoadConditionManager: DriveBC returned" << events.size() << "events";
    m_pendingRequests--;
    if (m_pendingRequests <= 0) processEvents();
}

void RoadConditionManager::processEvents()
{
    m_routeEvents.clear();

    // Filter events near route (within 10km of any route sample point)
    for (const auto &ev : m_allEvents) {
        if (isNearRoute(ev.lat, ev.lon, 10.0)) {
            m_routeEvents.append(ev);
        }
    }

    qDebug() << "RoadConditionManager:" << m_allEvents.size() << "total events,"
             << m_routeEvents.size() << "near route";

    buildSummary();

    // Emit alerts for significant events near route
    for (const auto &ev : m_routeEvents) {
        if (ev.fullClosure) {
            emit alertDetected(QString("Road closure ahead on %1. %2")
                .arg(ev.roadName.isEmpty() ? "your route" : ev.roadName,
                     shortenDescription(ev.description)));
        } else if (ev.severity == "MAJOR" || ev.severity == "Major") {
            emit alertDetected(QString("Major %1 ahead on %2. %3")
                .arg(ev.eventType,
                     ev.roadName.isEmpty() ? "your route" : ev.roadName,
                     shortenDescription(ev.description)));
        }
    }
}

void RoadConditionManager::buildSummary()
{
    if (m_routeEvents.isEmpty()) {
        m_summary = "No significant road conditions on route";
    } else {
        QString summary;
        int count = 0;
        for (const auto &ev : m_routeEvents) {
            if (count >= 8) break; // Cap at 8 events for context
            summary += QString("- %1 on %2: %3")
                .arg(ev.eventType, ev.roadName, shortenDescription(ev.description));
            if (!ev.severity.isEmpty() && ev.severity != "None")
                summary += QString(" [%1]").arg(ev.severity);
            summary += "\n";
            count++;
        }
        m_summary = summary;
    }

    emit summaryChanged();

    if (m_context) {
        m_context->setRoadConditionsSummary(m_summary);
    }

    qDebug() << "RoadConditionManager: Summary updated," << m_routeEvents.size() << "route events";
}

bool RoadConditionManager::isNearRoute(double lat, double lon, double radiusKm) const
{
    // Check against GPS position first
    if (m_context && m_context->gpsLatitude() != 0.0) {
        if (haversineKm(lat, lon, m_context->gpsLatitude(), m_context->gpsLongitude()) < radiusKm) {
            return true;
        }
    }

    // Check against sampled route points
    for (const auto &pt : m_routePoints) {
        if (haversineKm(lat, lon, pt.lat, pt.lon) < radiusKm) {
            return true;
        }
    }
    return false;
}

double RoadConditionManager::haversineKm(double lat1, double lon1, double lat2, double lon2) const
{
    const double R = 6371.0;
    double dLat = qDegreesToRadians(lat2 - lat1);
    double dLon = qDegreesToRadians(lon2 - lon1);
    double a = qSin(dLat / 2) * qSin(dLat / 2)
             + qCos(qDegreesToRadians(lat1)) * qCos(qDegreesToRadians(lat2))
             * qSin(dLon / 2) * qSin(dLon / 2);
    return R * 2 * qAtan2(qSqrt(a), qSqrt(1 - a));
}

QString RoadConditionManager::shortenDescription(const QString &desc) const
{
    // Strip "Activities:" boilerplate and truncate for TTS
    QString s = desc;
    int actIdx = s.indexOf("Activities:");
    if (actIdx > 0) s = s.left(actIdx).trimmed();

    // Remove "Next update time..." boilerplate
    int nextIdx = s.indexOf("Next update time");
    if (nextIdx > 0) s = s.left(nextIdx).trimmed();

    // Remove "Last updated..." boilerplate
    int lastIdx = s.indexOf("Last updated");
    if (lastIdx > 0) s = s.left(lastIdx).trimmed();

    // Cap length for TTS
    if (s.length() > 150) s = s.left(147) + "...";
    return s;
}
