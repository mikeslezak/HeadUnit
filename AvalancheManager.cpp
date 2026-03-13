#include "AvalancheManager.h"
#include "ContextAggregator.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QtMath>

AvalancheManager::AvalancheManager(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    connect(m_network, &QNetworkAccessManager::finished,
            this, &AvalancheManager::onForecastReply);

    // Refresh avalanche forecasts every 30 minutes
    m_refreshTimer->setInterval(30 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &AvalancheManager::refreshForecasts);

    qDebug() << "AvalancheManager: Initialized";
}

void AvalancheManager::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }

void AvalancheManager::setRouteCoordinates(const QJsonArray &coordinates, double durationSec)
{
    if (coordinates.size() < 2) {
        clearRoute();
        return;
    }

    ++m_generation;
    sampleMountainPoints(coordinates, durationSec);

    m_active = true;
    emit activeChanged();

    fetchForecasts();

    m_refreshTimer->start();
    qDebug() << "AvalancheManager: Tracking" << m_points.size() << "points along route";
}

void AvalancheManager::clearRoute()
{
    ++m_generation;
    m_active = false;
    m_points.clear();
    m_summary.clear();
    m_highestDanger.clear();
    m_refreshTimer->stop();
    emit activeChanged();
    emit summaryChanged();

    if (m_context) {
        m_context->setAvalancheSummary(QString());
    }
    qDebug() << "AvalancheManager: Route cleared";
}

void AvalancheManager::sampleMountainPoints(const QJsonArray &coordinates, double durationSec)
{
    m_points.clear();

    int numCoords = coordinates.size();
    // Sample 8 evenly-spaced points along the route (including start and end)
    int numSamples = qMin(8, numCoords);
    if (numSamples < 2) numSamples = 2;

    for (int i = 0; i < numSamples; ++i) {
        double fraction = (numSamples == 1) ? 0.0 : static_cast<double>(i) / (numSamples - 1);
        int idx = qMin(static_cast<int>(fraction * (numCoords - 1)), numCoords - 1);

        QJsonArray coord = coordinates[idx].toArray();
        if (coord.size() < 2) continue;

        ForecastPoint pt;
        pt.lon = coord[0].toDouble();
        pt.lat = coord[1].toDouble();

        double etaHours = (durationSec * fraction) / 3600.0;

        if (i == 0) pt.locationLabel = "Start";
        else if (i == numSamples - 1) pt.locationLabel = "Destination";
        else pt.locationLabel = QString("%1h ahead").arg(etaHours, 0, 'f', 1);

        m_points.append(pt);
    }
}

void AvalancheManager::fetchForecasts()
{
    m_pendingRequests = m_points.size();

    for (int i = 0; i < m_points.size(); ++i) {
        const auto &pt = m_points[i];

        QString url = QString("https://api.avalanche.ca/forecasts/en/products/point?lat=%1&long=%2")
            .arg(pt.lat, 0, 'f', 4)
            .arg(pt.lon, 0, 'f', 4);

        QUrl requestUrl(url);
        QNetworkRequest req(requestUrl);
        req.setAttribute(QNetworkRequest::User, i);
        req.setAttribute(QNetworkRequest::UserMax, m_generation);
        m_network->get(req);
    }
}

void AvalancheManager::onForecastReply(QNetworkReply *reply)
{
    reply->deleteLater();

    // Discard stale replies from a previous route
    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    int idx = reply->request().attribute(QNetworkRequest::User).toInt();
    if (idx < 0 || idx >= m_points.size()) return;

    auto &pt = m_points[idx];

    if (reply->error() != QNetworkReply::NoError) {
        // 404 means no forecast zone for this point (non-mountain area) — expected
        if (reply->error() == QNetworkReply::ContentNotFoundError) {
            qDebug() << "AvalancheManager: No forecast zone for point" << idx
                     << "(lat:" << pt.lat << "lon:" << pt.lon << ")";
        } else {
            qWarning() << "AvalancheManager: Forecast fetch failed for point" << idx << reply->errorString();
        }
        pt.hasForecast = false;
        m_pendingRequests--;
        if (m_pendingRequests <= 0) buildSummary();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    QJsonObject report = root["report"].toObject();

    if (report.isEmpty()) {
        pt.hasForecast = false;
        m_pendingRequests--;
        if (m_pendingRequests <= 0) buildSummary();
        return;
    }

    // Parse danger ratings — use first entry (today)
    QJsonArray dangerRatings = report["dangerRatings"].toArray();
    if (!dangerRatings.isEmpty()) {
        QJsonObject today = dangerRatings[0].toObject();
        QJsonObject danger = today["dangerRating"].toObject();

        // Format is "N:Label" (e.g., "3:Considerable") — extract the number
        auto parseDanger = [](const QString &val) -> int {
            int colonIdx = val.indexOf(':');
            if (colonIdx > 0) {
                bool ok;
                int level = val.left(colonIdx).toInt(&ok);
                if (ok) return level;
            }
            return 0;
        };

        pt.dangerAlpine = parseDanger(danger["alp"].toString());
        pt.dangerTreeline = parseDanger(danger["tln"].toString());
        pt.dangerBelowTree = parseDanger(danger["btl"].toString());
        pt.hasForecast = true;
    }

    // Extract highlights — strip HTML tags for plain text
    QString highlights = report["highlights"].toString();
    if (!highlights.isEmpty()) {
        static QRegularExpression htmlTags("<[^>]*>");
        pt.highlights = highlights.remove(htmlTags).simplified();
    }

    m_pendingRequests--;
    if (m_pendingRequests <= 0) {
        buildSummary();
    }
}

void AvalancheManager::buildSummary()
{
    // Filter to points that have forecasts
    QList<const ForecastPoint *> active;
    for (const auto &pt : m_points) {
        if (pt.hasForecast) active.append(&pt);
    }

    if (active.isEmpty()) {
        m_summary.clear();
        m_highestDanger.clear();
        emit summaryChanged();
        if (m_context) {
            m_context->setAvalancheSummary(QString());
        }
        qDebug() << "AvalancheManager: No avalanche forecast zones along route";
        return;
    }

    // Find highest danger level across all points and elevation bands
    int highest = 0;
    const ForecastPoint *highestPoint = nullptr;
    for (const auto *pt : active) {
        int maxForPoint = qMax(pt->dangerAlpine, qMax(pt->dangerTreeline, pt->dangerBelowTree));
        if (maxForPoint > highest) {
            highest = maxForPoint;
            highestPoint = pt;
        }
    }
    m_highestDanger = dangerLevelName(highest);

    // Build summary text
    QString summary;
    for (const auto *pt : active) {
        summary += QString("- %1: Alpine %2, Treeline %3, Below Treeline %4\n")
            .arg(pt->locationLabel,
                 dangerLevelName(pt->dangerAlpine),
                 dangerLevelName(pt->dangerTreeline),
                 dangerLevelName(pt->dangerBelowTree));
    }

    m_summary = summary;
    emit summaryChanged();

    if (m_context) {
        m_context->setAvalancheSummary(summary);
    }

    // Alert if highest danger >= 4 (High)
    if (highest >= 4 && highestPoint) {
        QString timeRef = highestPoint->locationLabel;
        emit alertDetected(
            QString("Avalanche danger is %1 in the mountain pass area about %2 on your route. Exercise extreme caution")
                .arg(dangerLevelName(highest), timeRef));
    }

    qDebug() << "AvalancheManager: Summary updated," << active.size()
             << "forecast zones, highest danger:" << m_highestDanger;
}

void AvalancheManager::refreshForecasts()
{
    if (!m_active || m_points.isEmpty()) return;
    qDebug() << "AvalancheManager: Refreshing forecasts";
    fetchForecasts();
}

QString AvalancheManager::dangerLevelName(int level) const
{
    switch (level) {
        case 1: return "Low";
        case 2: return "Moderate";
        case 3: return "Considerable";
        case 4: return "High";
        case 5: return "Extreme";
        default: return "Unknown";
    }
}
