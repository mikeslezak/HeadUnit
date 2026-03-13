#include "BorderWaitManager.h"
#include "ContextAggregator.h"
#include "GeoUtils.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QtMath>

BorderWaitManager::BorderWaitManager(QObject *parent)
    : QObject(parent)
    , m_cbpNetwork(new QNetworkAccessManager(this))
    , m_cbsaNetwork(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    connect(m_cbpNetwork, &QNetworkAccessManager::finished,
            this, &BorderWaitManager::onCbpReply);
    connect(m_cbsaNetwork, &QNetworkAccessManager::finished,
            this, &BorderWaitManager::onCbsaReply);

    // Refresh every 10 minutes
    m_refreshTimer->setInterval(10 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &BorderWaitManager::fetchWaitTimes);

    qDebug() << "BorderWaitManager: Initialized";
}

void BorderWaitManager::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }

QList<BorderWaitManager::KnownCrossing> BorderWaitManager::knownCrossings()
{
    static QList<KnownCrossing> crossings = {
        { "Coutts/Sweetgrass",           49.0027, -111.9619, 331001 },
        { "Del Bonita/Whitlash",         49.0000, -112.7500, 0 },
        { "Carway/Piegan",               49.0053, -113.3858, 0 },
        { "Chief Mountain",              49.0000, -113.6000, 0 },
        { "Pacific Highway",             49.0044, -122.7342, 300401 },
        { "Douglas/Peace Arch",          49.0022, -122.7568, 300402 },
        { "Abbotsford-Huntingdon/Sumas", 49.0022, -122.2647, 300901 },
        { "Aldergrove/Lynden",           49.0025, -122.4853, 302301 },
        { "Osoyoos/Oroville",            49.0000, -119.4500, 0 },
        { "Kingsgate/Eastport",          49.0000, -116.1833, 0 },
        { "Roosville/Eureka",            49.0000, -115.0667, 0 },
        { "Waneta/Metaline Falls",       49.0000, -117.6167, 0 },
        { "Cascade/Laurier",             49.0000, -118.2167, 0 },
        { "Regway/Raymond",              49.0000, -107.5500, 0 },
        { "North Portal/Portal",         49.0000, -102.5500, 0 },
    };
    return crossings;
}

void BorderWaitManager::setRouteCoordinates(const QJsonArray &coordinates, double durationSec)
{
    Q_UNUSED(durationSec)

    if (coordinates.size() < 2) {
        clearRoute();
        return;
    }

    ++m_generation;
    sampleRoutePoints(coordinates);

    if (!isNearBorder()) {
        if (m_active) {
            m_active = false;
            emit activeChanged();
        }
        qDebug() << "BorderWaitManager: Route does not pass near any border crossing";
        return;
    }

    m_active = true;
    emit activeChanged();

    fetchWaitTimes();
    m_refreshTimer->start();

    qDebug() << "BorderWaitManager: Tracking route with" << m_routePoints.size() << "sample points near border";
}

void BorderWaitManager::clearRoute()
{
    ++m_generation;
    m_active = false;
    m_routePoints.clear();
    m_waitData.clear();
    m_summary.clear();
    m_nearestCrossing.clear();
    m_waitMinutes = -1;
    m_refreshTimer->stop();
    emit activeChanged();
    emit summaryChanged();

    if (m_context) {
        m_context->setBorderWaitSummary(QString());
    }
    qDebug() << "BorderWaitManager: Route cleared";
}

void BorderWaitManager::sampleRoutePoints(const QJsonArray &coordinates)
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

bool BorderWaitManager::isNearBorder() const
{
    const auto crossings = knownCrossings();
    for (const auto &pt : m_routePoints) {
        for (const auto &cx : crossings) {
            if (GeoUtils::haversineKm(pt.lat, pt.lon, cx.lat, cx.lon) < 50.0) {
                return true;
            }
        }
    }
    return false;
}

void BorderWaitManager::fetchWaitTimes()
{
    ++m_generation;
    m_waitData.clear();
    m_pendingRequests = 0;

    // US CBP — JSON wait times
    m_pendingRequests++;
    QUrl cbpUrl("https://bwt.cbp.gov/api/waittimes");
    QNetworkRequest cbpReq(cbpUrl);
    cbpReq.setRawHeader("Accept", "application/json");
    cbpReq.setAttribute(QNetworkRequest::UserMax, m_generation);
    m_cbpNetwork->get(cbpReq);

    // CBSA — CSV wait times
    m_pendingRequests++;
    QUrl cbsaUrl("https://www.cbsa-asfc.gc.ca/bwt-taf/bwt-eng.csv");
    QNetworkRequest cbsaReq(cbsaUrl);
    cbsaReq.setAttribute(QNetworkRequest::UserMax, m_generation);
    m_cbsaNetwork->get(cbsaReq);

    qDebug() << "BorderWaitManager: Fetching wait times from CBP and CBSA";
}

void BorderWaitManager::onCbpReply(QNetworkReply *reply)
{
    reply->deleteLater();

    // Discard stale replies from a previous route
    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "BorderWaitManager: CBP fetch failed:" << reply->errorString();
        m_pendingRequests--;
        if (m_pendingRequests <= 0) processResults();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray entries = doc.array();

    const auto crossings = knownCrossings();

    for (const QJsonValue &v : entries) {
        QJsonObject obj = v.toObject();
        int portNumber = obj["port_number"].toInt();

        // Match against known crossings by port number
        const KnownCrossing *matched = nullptr;
        for (const auto &cx : crossings) {
            if (cx.cbpPortNumber != 0 && cx.cbpPortNumber == portNumber) {
                matched = &cx;
                break;
            }
        }
        if (!matched) continue;

        WaitTimeData data;
        data.crossingName = matched->name;
        data.lat = matched->lat;
        data.lon = matched->lon;
        data.source = "CBP";

        // Commercial vehicle lanes
        QJsonObject commercial = obj["commercial_vehicle_lanes"].toObject();
        QJsonObject commStandard = commercial["standard_lanes"].toObject();
        data.commercialMinutes = commStandard["delay_minutes"].toInt(-1);
        int commLanes = commStandard["lanes_open"].toInt(0);

        // Passenger vehicle lanes
        QJsonObject passenger = obj["passenger_vehicle_lanes"].toObject();
        QJsonObject passStandard = passenger["standard_lanes"].toObject();
        data.passengerMinutes = passStandard["delay_minutes"].toInt(-1);
        int passLanes = passStandard["lanes_open"].toInt(0);

        data.lanesOpen = commLanes + passLanes;

        m_waitData.append(data);
    }

    qDebug() << "BorderWaitManager: CBP returned" << entries.size() << "entries,"
             << "matched" << m_waitData.size() << "known crossings";
    m_pendingRequests--;
    if (m_pendingRequests <= 0) processResults();
}

void BorderWaitManager::onCbsaReply(QNetworkReply *reply)
{
    reply->deleteLater();

    // Discard stale replies from a previous route
    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "BorderWaitManager: CBSA fetch failed:" << reply->errorString();
        m_pendingRequests--;
        if (m_pendingRequests <= 0) processResults();
        return;
    }

    QString csv = QString::fromUtf8(reply->readAll());
    QStringList lines = csv.split('\n', Qt::SkipEmptyParts);

    const auto crossings = knownCrossings();
    int matchCount = 0;

    // Skip header row
    for (int i = 1; i < lines.size(); ++i) {
        QStringList fields = lines[i].split(';');
        if (fields.size() < 5) continue;

        QString portName = fields[0].trimmed();
        QString lastUpdated = fields[2].trimmed();
        QString commercialFlow = fields[3].trimmed();
        QString travellerFlow = fields[4].trimmed();

        // Match against known crossings by name (case-insensitive contains)
        const KnownCrossing *matched = nullptr;
        for (const auto &cx : crossings) {
            if (portName.contains(cx.name.split('/').first(), Qt::CaseInsensitive) ||
                cx.name.contains(portName.split('/').first().trimmed(), Qt::CaseInsensitive)) {
                matched = &cx;
                break;
            }
        }
        if (!matched) continue;

        // Check if we already have CBP data for this crossing — skip duplicate
        bool alreadyHave = false;
        for (const auto &existing : m_waitData) {
            if (existing.crossingName == matched->name) {
                alreadyHave = true;
                break;
            }
        }
        if (alreadyHave) continue;

        WaitTimeData data;
        data.crossingName = matched->name;
        data.lat = matched->lat;
        data.lon = matched->lon;
        data.source = "CBSA";
        data.lastUpdated = lastUpdated;

        // Parse wait times from flow strings (e.g., "Not applicable", "No delay", "15 min. delay")
        auto parseMinutes = [](const QString &flow) -> int {
            if (flow.contains("No delay", Qt::CaseInsensitive)) return 0;
            if (flow.contains("Not applicable", Qt::CaseInsensitive)) return -1;
            // Try to extract numeric minutes
            QRegularExpression re("(\\d+)\\s*min");
            QRegularExpressionMatch match = re.match(flow);
            if (match.hasMatch()) return match.captured(1).toInt();
            return -1;
        };

        data.commercialMinutes = parseMinutes(commercialFlow);
        data.passengerMinutes = parseMinutes(travellerFlow);

        m_waitData.append(data);
        matchCount++;
    }

    qDebug() << "BorderWaitManager: CBSA returned" << lines.size() - 1 << "rows,"
             << "matched" << matchCount << "known crossings";
    m_pendingRequests--;
    if (m_pendingRequests <= 0) processResults();
}

void BorderWaitManager::processResults()
{
    // Filter wait data to crossings near the route (within 50km of any route point)
    QList<WaitTimeData> nearRoute;
    for (const auto &wd : m_waitData) {
        for (const auto &pt : m_routePoints) {
            if (GeoUtils::haversineKm(wd.lat, wd.lon, pt.lat, pt.lon) < 50.0) {
                nearRoute.append(wd);
                break;
            }
        }
    }

    // Find the nearest crossing to any route point
    double minDist = 999999.0;
    m_nearestCrossing.clear();
    m_waitMinutes = -1;

    for (const auto &wd : nearRoute) {
        for (const auto &pt : m_routePoints) {
            double dist = GeoUtils::haversineKm(wd.lat, wd.lon, pt.lat, pt.lon);
            if (dist < minDist) {
                minDist = dist;
                m_nearestCrossing = wd.crossingName;
                // Use passenger wait time as the primary metric, fall back to commercial
                m_waitMinutes = (wd.passengerMinutes >= 0) ? wd.passengerMinutes : wd.commercialMinutes;
            }
        }
    }

    m_waitData = nearRoute;

    qDebug() << "BorderWaitManager:" << nearRoute.size() << "crossings near route,"
             << "nearest:" << m_nearestCrossing << "wait:" << m_waitMinutes << "min";

    buildSummary();

    // Emit alerts for long commercial waits
    for (const auto &wd : nearRoute) {
        if (wd.commercialMinutes > 60) {
            emit alertDetected(QString("Border wait at %1 is currently %2 minutes for commercial vehicles")
                .arg(wd.crossingName)
                .arg(wd.commercialMinutes));
        }
    }
}

void BorderWaitManager::buildSummary()
{
    if (m_waitData.isEmpty()) {
        m_summary = "No border crossing wait time data available";
    } else {
        QString summary;
        for (const auto &wd : m_waitData) {
            summary += QString("- %1:").arg(wd.crossingName);

            if (wd.commercialMinutes >= 0) {
                summary += QString(" Commercial %1 min,").arg(wd.commercialMinutes);
            }
            if (wd.passengerMinutes >= 0) {
                summary += QString(" Passenger %1 min").arg(wd.passengerMinutes);
            }
            if (wd.lanesOpen > 0) {
                summary += QString(" (%1 lanes open)").arg(wd.lanesOpen);
            }
            summary += "\n";
        }
        m_summary = summary;
    }

    emit summaryChanged();

    if (m_context) {
        m_context->setBorderWaitSummary(m_summary);
    }

    qDebug() << "BorderWaitManager: Summary updated," << m_waitData.size() << "crossings";
}

