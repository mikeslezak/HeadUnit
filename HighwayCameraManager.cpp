#include "HighwayCameraManager.h"
#include "GeoUtils.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QtMath>

HighwayCameraManager::HighwayCameraManager(QObject *parent)
    : QObject(parent)
    , m_albertaNetwork(new QNetworkAccessManager(this))
    , m_drivebcNetwork(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    connect(m_albertaNetwork, &QNetworkAccessManager::finished,
            this, &HighwayCameraManager::onAlbertaReply);
    connect(m_drivebcNetwork, &QNetworkAccessManager::finished,
            this, &HighwayCameraManager::onDriveBCReply);

    // Refresh every 5 minutes
    m_refreshTimer->setInterval(5 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &HighwayCameraManager::fetchCameras);

    qDebug() << "HighwayCameraManager: Initialized";
}

void HighwayCameraManager::setRouteCoordinates(const QJsonArray &coordinates, double durationSec)
{
    Q_UNUSED(durationSec)

    if (coordinates.size() < 2) {
        clearRoute();
        return;
    }

    ++m_generation;
    sampleRoutePoints(coordinates);

    m_active = true;
    emit activeChanged();

    fetchCameras();
    m_refreshTimer->start();

    qDebug() << "HighwayCameraManager: Tracking route with" << m_routePoints.size() << "sample points";
}

void HighwayCameraManager::clearRoute()
{
    ++m_generation;
    m_active = false;
    m_routePoints.clear();
    m_allCameras.clear();
    m_routeCameras.clear();
    m_camerasJson = QJsonArray();
    m_refreshTimer->stop();
    emit activeChanged();
    emit camerasChanged();

    qDebug() << "HighwayCameraManager: Route cleared";
}

void HighwayCameraManager::sampleRoutePoints(const QJsonArray &coordinates)
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

void HighwayCameraManager::fetchCameras()
{
    ++m_generation;
    m_allCameras.clear();
    m_pendingRequests = 0;

    // 511 Alberta cameras
    m_pendingRequests++;
    QUrl abUrl("https://511.alberta.ca/api/v2/get/cameras");
    QNetworkRequest abReq(abUrl);
    abReq.setRawHeader("Accept", "application/json");
    abReq.setAttribute(QNetworkRequest::UserMax, m_generation);
    m_albertaNetwork->get(abReq);

    // DriveBC webcams
    m_pendingRequests++;
    QUrl bcUrl("https://www.drivebc.ca/api/webcams/");
    QNetworkRequest bcReq(bcUrl);
    bcReq.setRawHeader("Accept", "application/json");
    bcReq.setAttribute(QNetworkRequest::UserMax, m_generation);
    m_drivebcNetwork->get(bcReq);

    qDebug() << "HighwayCameraManager: Fetching cameras from 511AB and DriveBC";
}

void HighwayCameraManager::onAlbertaReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "HighwayCameraManager: Alberta fetch failed:" << reply->errorString();
        m_pendingRequests--;
        if (m_pendingRequests <= 0) processResults();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray cameras = doc.array();

    for (const QJsonValue &v : cameras) {
        QJsonObject obj = v.toObject();

        double lat = obj["Latitude"].toDouble();
        double lon = obj["Longitude"].toDouble();
        if (lat == 0.0 && lon == 0.0) continue;

        // Get image URL from first active view
        QJsonArray views = obj["Views"].toArray();
        QString imageUrl;
        for (const QJsonValue &view : views) {
            QJsonObject viewObj = view.toObject();
            QString status = viewObj["Status"].toString();
            if (!status.isEmpty() && status != "Active") continue;
            imageUrl = viewObj["Url"].toString();
            break;
        }
        if (imageUrl.isEmpty()) continue;

        Camera cam;
        cam.id = QString("ab-%1").arg(obj["Id"].toString());
        cam.name = obj["Name"].toString();
        cam.imageUrl = imageUrl;
        cam.lat = lat;
        cam.lon = lon;
        cam.direction = obj["Direction"].toString();
        cam.roadName = obj["Roadway"].toString();
        cam.source = "511AB";

        m_allCameras.append(cam);
    }

    qDebug() << "HighwayCameraManager: Alberta returned" << cameras.size() << "cameras";
    m_pendingRequests--;
    if (m_pendingRequests <= 0) processResults();
}

void HighwayCameraManager::onDriveBCReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "HighwayCameraManager: DriveBC fetch failed:" << reply->errorString();
        m_pendingRequests--;
        if (m_pendingRequests <= 0) processResults();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray webcams = doc.array();

    for (const QJsonValue &v : webcams) {
        QJsonObject obj = v.toObject();

        // Skip disabled cameras
        if (!obj["isOn"].toBool()) continue;

        QJsonObject location = obj["location"].toObject();
        double lat = location["latitude"].toDouble();
        double lon = location["longitude"].toDouble();
        if (lat == 0.0 && lon == 0.0) continue;

        QJsonObject links = obj["links"].toObject();
        QString imageUrl = links["imageDisplay"].toString();
        if (imageUrl.isEmpty()) continue;

        Camera cam;
        cam.id = QString("bc-%1").arg(obj["id"].toInt());
        cam.name = obj["camName"].toString();
        cam.imageUrl = imageUrl;
        cam.lat = lat;
        cam.lon = lon;
        cam.direction = obj["caption"].toString();
        cam.roadName = obj["highway"].toString();
        cam.source = "DriveBC";

        m_allCameras.append(cam);
    }

    qDebug() << "HighwayCameraManager: DriveBC returned" << webcams.size() << "webcams";
    m_pendingRequests--;
    if (m_pendingRequests <= 0) processResults();
}

void HighwayCameraManager::processResults()
{
    m_routeCameras.clear();

    // Filter cameras near route (within 15km of any route sample point)
    for (const auto &cam : m_allCameras) {
        if (isNearRoute(cam.lat, cam.lon, 15.0)) {
            m_routeCameras.append(cam);
        }
    }

    // Build QJsonArray for QML consumption
    QJsonArray arr;
    for (const auto &cam : m_routeCameras) {
        QJsonObject obj;
        obj["id"] = cam.id;
        obj["name"] = cam.name;
        obj["imageUrl"] = cam.imageUrl;
        obj["lat"] = cam.lat;
        obj["lon"] = cam.lon;
        obj["direction"] = cam.direction;
        obj["roadName"] = cam.roadName;
        obj["source"] = cam.source;
        arr.append(obj);
    }
    m_camerasJson = arr;

    emit camerasChanged();

    qDebug() << "HighwayCameraManager:" << m_allCameras.size() << "total cameras,"
             << m_routeCameras.size() << "near route";
}

bool HighwayCameraManager::isNearRoute(double lat, double lon, double radiusKm) const
{
    // Check against sampled route points
    for (const auto &pt : m_routePoints) {
        if (GeoUtils::haversineKm(lat, lon, pt.lat, pt.lon) < radiusKm) {
            return true;
        }
    }
    return false;
}

