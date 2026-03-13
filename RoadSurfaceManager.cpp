#include "RoadSurfaceManager.h"
#include "ContextAggregator.h"
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

void RoadSurfaceManager::setRouteCoordinates(const QJsonArray &coordinates, double durationSec)
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

    qDebug() << "RoadSurfaceManager: Tracking route with" << m_routePoints.size() << "sample points";
}

void RoadSurfaceManager::clearRoute()
{
    m_active = false;
    m_routeCoordinates = QJsonArray();
    m_routePoints.clear();
    m_allReports.clear();
    m_routeReports.clear();
    m_summary.clear();
    m_refreshTimer->stop();
    emit activeChanged();
    emit summaryChanged();

    if (m_context) {
        m_context->setRoadSurfaceSummary(QString());
    }
    qDebug() << "RoadSurfaceManager: Route cleared";
}

void RoadSurfaceManager::sampleRoutePoints(const QJsonArray &coordinates)
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

void RoadSurfaceManager::fetchConditions()
{
    m_allReports.clear();
    m_pendingRequests = 0;

    // 511 Alberta Winter Roads — returns all roads, filter by proximity later
    m_pendingRequests++;
    QUrl abUrl("https://511.alberta.ca/api/v2/get/winterroads");
    QNetworkRequest abReq(abUrl);
    abReq.setRawHeader("Accept", "application/json");
    m_albertaNetwork->get(abReq);

    // DriveBC Weather Stations — returns all stations, filter by proximity later
    m_pendingRequests++;
    QUrl bcUrl("https://www.drivebc.ca/api/weather/current/");
    QNetworkRequest bcReq(bcUrl);
    bcReq.setRawHeader("Accept", "application/json");
    m_drivebcNetwork->get(bcReq);

    qDebug() << "RoadSurfaceManager: Fetching surface conditions from 511AB + DriveBC";
}

void RoadSurfaceManager::onAlbertaReply(QNetworkReply *reply)
{
    reply->deleteLater();

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

    // Filter reports near route (within 15km of any route sample point)
    for (const auto &rpt : m_allReports) {
        if (isNearRoute(rpt.lat, rpt.lon, 15.0)) {
            m_routeReports.append(rpt);
        }
    }

    qDebug() << "RoadSurfaceManager:" << m_allReports.size() << "total reports,"
             << m_routeReports.size() << "near route";

    buildSummary();

    // Emit alerts for dangerous conditions near route
    for (const auto &rpt : m_routeReports) {
        bool dangerousCondition =
            rpt.condition.contains("Ice", Qt::CaseInsensitive) ||
            rpt.condition.contains("Covered Snow", Qt::CaseInsensitive) ||
            rpt.condition.contains("Packed", Qt::CaseInsensitive);

        if (dangerousCondition) {
            emit alertDetected(QString("Icy road conditions reported on %1 ahead")
                .arg(rpt.roadName.isEmpty() ? "your route" : rpt.roadName));
        }

        if (!std::isnan(rpt.pavementTempC) && rpt.pavementTempC < -5.0) {
            emit alertDetected(QString("Road surface temperature is %1°C near %2 — watch for black ice")
                .arg(rpt.pavementTempC, 0, 'f', 0)
                .arg(rpt.roadName.isEmpty() ? "your route" : rpt.roadName));
        }
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

bool RoadSurfaceManager::isNearRoute(double lat, double lon, double radiusKm) const
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

double RoadSurfaceManager::haversineKm(double lat1, double lon1, double lat2, double lon2) const
{
    const double R = 6371.0;
    double dLat = qDegreesToRadians(lat2 - lat1);
    double dLon = qDegreesToRadians(lon2 - lon1);
    double a = qSin(dLat / 2) * qSin(dLat / 2)
             + qCos(qDegreesToRadians(lat1)) * qCos(qDegreesToRadians(lat2))
             * qSin(dLon / 2) * qSin(dLon / 2);
    return R * 2 * qAtan2(qSqrt(a), qSqrt(1 - a));
}
