#include "RoadSurfaceManager.h"
#include "ContextAggregator.h"
#include "GeoUtils.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>
#include <cmath>

RoadSurfaceManager::RoadSurfaceManager(QObject *parent)
    : QObject(parent)
    , m_albertaNetwork(new QNetworkAccessManager(this))
    , m_drivebcNetwork(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    connect(m_albertaNetwork, &QNetworkAccessManager::finished,
            this, &RoadSurfaceManager::onAlbertaReply);
    connect(m_drivebcNetwork, &QNetworkAccessManager::finished,
            this, &RoadSurfaceManager::onDriveBCReply);

    // Refresh every 10 minutes
    m_refreshTimer->setInterval(10 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &RoadSurfaceManager::fetchConditions);

    qDebug() << "RoadSurfaceManager: Initialized";
}

void RoadSurfaceManager::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }

void RoadSurfaceManager::setRouteCoordinates(const QJsonArray &coordinates, double durationSec, bool silent)
{
    Q_UNUSED(durationSec)

    if (coordinates.size() < 2) {
        clearRoute();
        return;
    }

    m_suppressNextAlert = silent;
    ++m_generation;
    m_routeCoordinates = coordinates;

    m_active = true;
    emit activeChanged();

    fetchConditions();
    m_refreshTimer->start();

    qDebug() << "RoadSurfaceManager: Tracking route with" << coordinates.size() << "coordinates";
}

void RoadSurfaceManager::clearRoute()
{
    ++m_generation;
    m_active = false;
    m_routeCoordinates = QJsonArray();
    m_allReports.clear();
    m_routeReports.clear();
    m_summary.clear();
    m_refreshTimer->stop();
    emit activeChanged();
    emit summaryChanged();

    m_lastAlertText.clear();
    m_suppressNextAlert = false;

    if (m_context) {
        m_context->setRoadSurfaceSummary(QString());
    }
    qDebug() << "RoadSurfaceManager: Route cleared";
}

void RoadSurfaceManager::fetchConditions()
{
    ++m_generation;
    m_allReports.clear();
    m_pendingRequests = 0;

    // 511 Alberta Winter Roads — returns all roads, filter by proximity later
    m_pendingRequests++;
    QUrl abUrl("https://511.alberta.ca/api/v2/get/winterroads");
    QNetworkRequest abReq(abUrl);
    abReq.setRawHeader("Accept", "application/json");
    abReq.setAttribute(QNetworkRequest::UserMax, m_generation);
    m_albertaNetwork->get(abReq);

    // DriveBC Weather Stations — returns all stations, filter by proximity later
    m_pendingRequests++;
    QUrl bcUrl("https://www.drivebc.ca/api/weather/current/");
    QNetworkRequest bcReq(bcUrl);
    bcReq.setRawHeader("Accept", "application/json");
    bcReq.setAttribute(QNetworkRequest::UserMax, m_generation);
    m_drivebcNetwork->get(bcReq);

    qDebug() << "RoadSurfaceManager: Fetching surface conditions from 511AB + DriveBC";
}

void RoadSurfaceManager::onAlbertaReply(QNetworkReply *reply)
{
    reply->deleteLater();

    // Discard stale replies from a previous route
    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "RoadSurfaceManager: Alberta fetch failed:" << reply->errorString();
        m_pendingRequests--;
        if (m_pendingRequests <= 0) processResults();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray roads = doc.array();

    for (const QJsonValue &v : roads) {
        QJsonObject obj = v.toObject();

        QString encodedPolyline = obj["EncodedPolyline"].toString();
        if (encodedPolyline.isEmpty()) continue;

        QPair<double, double> firstPt = decodePolylineFirstPoint(encodedPolyline);
        if (firstPt.first == 0.0 && firstPt.second == 0.0) continue;

        SurfaceReport rpt;
        rpt.roadName = obj["RoadwayName"].toString();
        rpt.lat = firstPt.first;
        rpt.lon = firstPt.second;
        rpt.source = "511AB";
        rpt.pavementTempC = std::numeric_limits<double>::quiet_NaN();

        // Normalize condition strings
        QString condition = obj["Primary Condition"].toString().trimmed();
        if (condition == "Cvd Snw")
            condition = "Covered Snow";
        else if (condition == "Ptly Cvd Ice")
            condition = "Partly Covered Ice";
        else if (condition == "Ptly Cvd Snw")
            condition = "Partly Covered Snow";
        rpt.condition = condition;

        if (rpt.condition.isEmpty() || rpt.condition == "No Report") continue;

        m_allReports.append(rpt);
    }

    qDebug() << "RoadSurfaceManager: Alberta returned" << roads.size() << "winter road entries";
    m_pendingRequests--;
    if (m_pendingRequests <= 0) processResults();
}

void RoadSurfaceManager::onDriveBCReply(QNetworkReply *reply)
{
    reply->deleteLater();

    // Discard stale replies from a previous route
    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "RoadSurfaceManager: DriveBC fetch failed:" << reply->errorString();
        m_pendingRequests--;
        if (m_pendingRequests <= 0) processResults();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray stations = doc.array();

    for (const QJsonValue &v : stations) {
        QJsonObject obj = v.toObject();

        // Extract coordinates from GeoJSON location object
        QJsonObject location = obj["location"].toObject();
        QJsonArray coords = location["coordinates"].toArray();
        if (coords.size() < 2) continue;

        SurfaceReport rpt;
        rpt.lon = coords[0].toDouble();
        rpt.lat = coords[1].toDouble();
        rpt.source = "DriveBC";

        // Road temperature
        QJsonValue roadTempVal = obj["road_temperature"];
        if (roadTempVal.isDouble()) {
            rpt.pavementTempC = roadTempVal.toDouble();
        } else if (roadTempVal.isString() && !roadTempVal.toString().isEmpty()) {
            bool ok = false;
            double temp = roadTempVal.toString().toDouble(&ok);
            rpt.pavementTempC = ok ? temp : std::numeric_limits<double>::quiet_NaN();
        } else {
            rpt.pavementTempC = std::numeric_limits<double>::quiet_NaN();
        }

        rpt.condition = obj["road_condition"].toString().trimmed();
        rpt.roadName = obj["station_name"].toString();
        if (rpt.roadName.isEmpty()) {
            rpt.roadName = obj["name"].toString();
        }

        if (rpt.lat == 0.0 && rpt.lon == 0.0) continue;

        m_allReports.append(rpt);
    }

    qDebug() << "RoadSurfaceManager: DriveBC returned" << stations.size() << "weather stations";
    m_pendingRequests--;
    if (m_pendingRequests <= 0) processResults();
}

QPair<double, double> RoadSurfaceManager::decodePolylineFirstPoint(const QString &encoded)
{
    // Google Encoded Polyline Algorithm — decode only the first lat/lon pair
    // Each value: read 5-bit chunks (char - 63), bottom 5 bits, continue if bit 0x20 set
    // Then zigzag decode: (result >> 1) ^ -(result & 1), divide by 1e5

    int index = 0;
    int len = encoded.length();
    double lat = 0.0;
    double lon = 0.0;

    // Decode latitude
    if (index >= len) return qMakePair(0.0, 0.0);
    int shift = 0;
    int result = 0;
    int b;
    do {
        if (index >= len) return qMakePair(0.0, 0.0);
        b = encoded.at(index++).toLatin1() - 63;
        result |= (b & 0x1F) << shift;
        shift += 5;
    } while (b >= 0x20);
    lat = ((result & 1) ? ~(result >> 1) : (result >> 1)) / 1e5;

    // Decode longitude
    if (index >= len) return qMakePair(0.0, 0.0);
    shift = 0;
    result = 0;
    do {
        if (index >= len) return qMakePair(0.0, 0.0);
        b = encoded.at(index++).toLatin1() - 63;
        result |= (b & 0x1F) << shift;
        shift += 5;
    } while (b >= 0x20);
    lon = ((result & 1) ? ~(result >> 1) : (result >> 1)) / 1e5;

    return qMakePair(lat, lon);
}

void RoadSurfaceManager::processResults()
{
    m_routeReports.clear();

    // Filter reports near route (within 200m of route polyline)
    for (const auto &rpt : m_allReports) {
        if (isNearRoute(rpt.lat, rpt.lon)) {
            m_routeReports.append(rpt);
        }
    }

    qDebug() << "RoadSurfaceManager:" << m_allReports.size() << "total reports,"
             << m_routeReports.size() << "near route";

    buildSummary();

    // Build alert text from dangerous conditions
    QString combinedAlert;
    for (const auto &rpt : m_routeReports) {
        QString road = rpt.roadName.isEmpty() ? "your route" : rpt.roadName;

        bool dangerousCondition =
            rpt.condition.contains("Ice", Qt::CaseInsensitive) ||
            rpt.condition.contains("Covered Snow", Qt::CaseInsensitive) ||
            rpt.condition.contains("Packed", Qt::CaseInsensitive);

        if (dangerousCondition) {
            combinedAlert += QString("Icy road conditions reported on %1 ahead. ").arg(road);
        }

        if (!std::isnan(rpt.pavementTempC) && rpt.pavementTempC < -5.0) {
            combinedAlert += QString("Road surface temperature is %1°C near %2 — watch for black ice. ")
                .arg(rpt.pavementTempC, 0, 'f', 0)
                .arg(road);
        }
    }

    // --- Change detection: only emit alertDetected when alert text differs ---
    if (m_suppressNextAlert) {
        m_suppressNextAlert = false;
        m_lastAlertText = combinedAlert;
        qDebug() << "RoadSurfaceManager: Alert suppressed (silent mode)";
        return;
    }

    if (combinedAlert == m_lastAlertText) {
        qDebug() << "RoadSurfaceManager: No change in surface conditions, skipping alert";
        return;
    }

    m_lastAlertText = combinedAlert;
    if (!combinedAlert.isEmpty()) {
        emit alertDetected(combinedAlert.trimmed());
    }
}

void RoadSurfaceManager::buildSummary()
{
    if (m_routeReports.isEmpty()) {
        m_summary = "No surface condition reports near route";
    } else {
        QString summary;
        int count = 0;
        for (const auto &rpt : m_routeReports) {
            if (count >= 8) break; // Cap at 8 entries for context
            summary += QString("- %1: %2").arg(
                rpt.roadName.isEmpty() ? rpt.source : rpt.roadName,
                rpt.condition);
            if (!std::isnan(rpt.pavementTempC)) {
                summary += QString(", pavement %1°C").arg(rpt.pavementTempC, 0, 'f', 1);
            }
            summary += QString(" [%1]").arg(rpt.source);
            summary += "\n";
            count++;
        }
        m_summary = summary;
    }

    emit summaryChanged();

    if (m_context) {
        m_context->setRoadSurfaceSummary(m_summary);
    }

    qDebug() << "RoadSurfaceManager: Summary updated," << m_routeReports.size() << "route reports";
}

bool RoadSurfaceManager::isNearRoute(double lat, double lon) const
{
    // Check perpendicular distance from the report to each segment of the actual route polyline.
    // Only reports within 200m of the route line itself are considered "near route".
    const double ON_ROUTE_THRESHOLD_KM = 0.2; // 200 meters

    int numCoords = m_routeCoordinates.size();
    if (numCoords >= 2) {
        // Walk every segment of the full route polyline
        // For performance, step through every 5th coordinate (routes can have thousands of points)
        int step = qMax(1, numCoords / 500);
        for (int i = 0; i < numCoords - step; i += step) {
            QJsonArray a = m_routeCoordinates[i].toArray();
            int j = qMin(i + step, numCoords - 1);
            QJsonArray b = m_routeCoordinates[j].toArray();
            if (a.size() >= 2 && b.size() >= 2) {
                double dist = GeoUtils::pointToSegmentDistanceKm(
                    lat, lon,
                    a[1].toDouble(), a[0].toDouble(),  // lat, lon (GeoJSON is lon,lat)
                    b[1].toDouble(), b[0].toDouble());
                if (dist < ON_ROUTE_THRESHOLD_KM) {
                    return true;
                }
            }
        }
        return false;
    }

    // No route — check GPS position with 200m radius
    if (m_context && m_context->gpsLatitude() != 0.0) {
        return GeoUtils::haversineKm(lat, lon, m_context->gpsLatitude(), m_context->gpsLongitude()) < ON_ROUTE_THRESHOLD_KM;
    }

    return false;
}


