#include "SpeedLimitManager.h"
#include "ContextAggregator.h"
#include "GeoUtils.h"
#include <QDebug>
#include <QJsonObject>
#include <QtMath>

SpeedLimitManager::SpeedLimitManager(QObject *parent)
    : QObject(parent)
{
    qDebug() << "SpeedLimitManager: Initialized";
}

void SpeedLimitManager::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }

void SpeedLimitManager::setSpeedLimitData(const QJsonArray &maxspeedAnnotations, const QJsonArray &routeCoordinates, double durationSec)
{
    Q_UNUSED(durationSec)

    m_segments.clear();
    m_lastSegmentIndex = 0;
    m_speedingStarted = false;
    m_speedingAlertSent = false;

    int numCoords = routeCoordinates.size();
    int numAnnotations = maxspeedAnnotations.size();

    // Annotations array has N-1 elements for N coordinates (one per segment)
    if (numCoords < 2 || numAnnotations < 1) {
        clearRoute();
        return;
    }

    for (int i = 0; i < numAnnotations && i < numCoords - 1; ++i) {
        QJsonObject ann = maxspeedAnnotations[i].toObject();

        // Get the two endpoints of this segment
        QJsonArray coordA = routeCoordinates[i].toArray();
        QJsonArray coordB = routeCoordinates[i + 1].toArray();
        if (coordA.size() < 2 || coordB.size() < 2) continue;

        // Store midpoint of segment (Mapbox coords are [lon, lat])
        double latA = coordA[1].toDouble();
        double lonA = coordA[0].toDouble();
        double latB = coordB[1].toDouble();
        double lonB = coordB[0].toDouble();

        SpeedSegment seg;
        seg.lat = (latA + latB) / 2.0;
        seg.lon = (lonA + lonB) / 2.0;
        seg.speedKmh = 0;
        seg.isNone = false;

        if (ann.contains("none") && ann["none"].toBool()) {
            // "none" means no speed limit (e.g. autobahn)
            seg.isNone = true;
        } else if (ann.contains("unknown") && ann["unknown"].toBool()) {
            // Unknown speed limit
            seg.speedKmh = 0;
        } else if (ann.contains("speed")) {
            int speed = ann["speed"].toInt();
            QString unit = ann["unit"].toString();
            if (unit == "mph") {
                seg.speedKmh = qRound(speed * 1.60934);
            } else {
                seg.speedKmh = speed; // km/h is the default
            }
        }

        m_segments.append(seg);
    }

    m_active = true;
    emit activeChanged();

    buildSummary();
    qDebug() << "SpeedLimitManager: Loaded" << m_segments.size() << "speed segments";
}

void SpeedLimitManager::updateGpsPosition(double lat, double lon, double speedKmh)
{
    if (!m_active || m_segments.isEmpty()) return;

    // Find nearest segment using sequential search from last known position
    int bestIdx = -1;
    double bestDist = 1e9;

    // Search forward from last index, up to 30 segments
    int searchEnd = qMin(m_lastSegmentIndex + 30, m_segments.size());
    for (int i = m_lastSegmentIndex; i < searchEnd; ++i) {
        double d = GeoUtils::haversineKm(lat, lon, m_segments[i].lat, m_segments[i].lon);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }

    // If nothing close found in forward window, do a full search
    if (bestIdx < 0 || bestDist > 1.0) {
        for (int i = 0; i < m_segments.size(); ++i) {
            double d = GeoUtils::haversineKm(lat, lon, m_segments[i].lat, m_segments[i].lon);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
        }
    }

    if (bestIdx < 0) return;

    m_lastSegmentIndex = bestIdx;
    const SpeedSegment &seg = m_segments[bestIdx];

    if (seg.speedKmh > 0 && !seg.isNone) {
        // Valid speed limit
        if (m_currentSpeedLimit != seg.speedKmh) {
            m_currentSpeedLimit = seg.speedKmh;
            emit currentSpeedLimitChanged();
        }

        // Check speeding (10 km/h grace)
        bool nowSpeeding = speedKmh > m_currentSpeedLimit + 10;

        if (nowSpeeding) {
            if (!m_speedingStarted) {
                m_speedingStarted = true;
                m_speedingTimer.start();
                m_speedingAlertSent = false;
            }

            // Alert after sustained speeding (10+ seconds)
            if (m_speedingStarted && m_speedingTimer.elapsed() > 10000 && !m_speedingAlertSent) {
                int over = qRound(speedKmh) - m_currentSpeedLimit;
                emit alertDetected(QString("You're going %1 km/h over the speed limit of %2 km/h")
                    .arg(over).arg(m_currentSpeedLimit));
                m_speedingAlertSent = true;
            }
        } else {
            m_speedingStarted = false;
            m_speedingAlertSent = false;
        }

        if (m_speeding != nowSpeeding) {
            m_speeding = nowSpeeding;
            emit speedingChanged();
        }
    } else {
        // No speed limit data for this segment
        if (m_currentSpeedLimit != 0) {
            m_currentSpeedLimit = 0;
            emit currentSpeedLimitChanged();
        }
        if (m_speeding) {
            m_speeding = false;
            emit speedingChanged();
        }
    }
}

void SpeedLimitManager::clearRoute()
{
    m_active = false;
    m_segments.clear();
    m_lastSegmentIndex = 0;
    m_currentSpeedLimit = 0;
    m_speeding = false;
    m_speedingStarted = false;
    m_speedingAlertSent = false;
    m_summary.clear();
    emit activeChanged();
    emit currentSpeedLimitChanged();
    emit speedingChanged();
    emit summaryChanged();

    if (m_context) {
        m_context->setSpeedLimitSummary(QString());
    }
    qDebug() << "SpeedLimitManager: Route cleared";
}

void SpeedLimitManager::buildSummary()
{
    int withData = 0;
    int withoutData = 0;
    int minSpeed = INT_MAX;
    int maxSpeed = 0;

    for (const auto &seg : m_segments) {
        if (seg.speedKmh > 0 && !seg.isNone) {
            withData++;
            minSpeed = qMin(minSpeed, seg.speedKmh);
            maxSpeed = qMax(maxSpeed, seg.speedKmh);
        } else {
            withoutData++;
        }
    }

    int total = withData + withoutData;
    if (total == 0) {
        m_summary = "No speed limit data available";
    } else if (withData == 0) {
        m_summary = "No speed limit data available for this route";
    } else {
        int pct = qRound(100.0 * withData / total);
        if (minSpeed == maxSpeed) {
            m_summary = QString("Speed limit: %1 km/h, data available for %2% of route")
                .arg(minSpeed).arg(pct);
        } else {
            m_summary = QString("Speed limits: %1-%2 km/h, data available for %3% of route")
                .arg(minSpeed).arg(maxSpeed).arg(pct);
        }
    }

    emit summaryChanged();

    if (m_context) {
        m_context->setSpeedLimitSummary(m_summary);
    }

    qDebug() << "SpeedLimitManager: Summary updated," << m_segments.size() << "segments,"
             << withData << "with data";
}

