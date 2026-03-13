#include "PlacesSearchManager.h"
#include "ContextAggregator.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QtMath>
#include <QRegularExpression>

PlacesSearchManager::PlacesSearchManager(QObject *parent)
    : QObject(parent)
    , m_mapboxNetwork(new QNetworkAccessManager(this))
    , m_googleNetwork(new QNetworkAccessManager(this))
    , m_geocodeNetwork(new QNetworkAccessManager(this))
{
    connect(m_mapboxNetwork, &QNetworkAccessManager::finished,
            this, &PlacesSearchManager::onMapboxReply);
    connect(m_googleNetwork, &QNetworkAccessManager::finished,
            this, &PlacesSearchManager::onGoogleReply);
    connect(m_geocodeNetwork, &QNetworkAccessManager::finished,
            this, &PlacesSearchManager::onGeocodeReply);
    qDebug() << "PlacesSearchManager: Initialized";
}

void PlacesSearchManager::setMapboxToken(const QString &token) { m_mapboxToken = token; }
void PlacesSearchManager::setGoogleApiKey(const QString &key) { m_googleApiKey = key; }
void PlacesSearchManager::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }

void PlacesSearchManager::searchPlaces(const QString &query, const QString &category)
{
    if (m_mapboxToken.isEmpty()) {
        emit searchFailed("Mapbox token not configured");
        return;
    }

    double lat = m_context ? m_context->gpsLatitude() : 0.0;
    double lon = m_context ? m_context->gpsLongitude() : 0.0;

    if (lat == 0.0 && lon == 0.0) {
        emit searchFailed("No GPS location available for nearby search");
        return;
    }

    qDebug() << "PlacesSearchManager: Searching for" << query << "near" << lat << lon;

    // Mapbox Geocoding v5 with POI type + proximity bias
    QString url = QString("https://api.mapbox.com/geocoding/v5/mapbox.places/%1.json")
        .arg(QUrl::toPercentEncoding(query));

    QUrlQuery params;
    params.addQueryItem("access_token", m_mapboxToken);
    params.addQueryItem("proximity", QString("%1,%2").arg(lon, 0, 'f', 6).arg(lat, 0, 'f', 6));
    params.addQueryItem("types", "poi");
    params.addQueryItem("limit", "5");
    if (!category.isEmpty()) {
        params.addQueryItem("categories", category);
    }

    QUrl requestUrl(url);
    requestUrl.setQuery(params);

    m_mapboxNetwork->get(QNetworkRequest(requestUrl));
}

void PlacesSearchManager::onMapboxReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit searchFailed("Search failed: " + reply->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray features = doc.object()["features"].toArray();

    if (features.isEmpty()) {
        emit searchCompleted("No results found nearby.");
        return;
    }

    double myLat = m_context ? m_context->gpsLatitude() : 0.0;
    double myLon = m_context ? m_context->gpsLongitude() : 0.0;

    QList<PlaceResult> results;
    for (const QJsonValue &f : features) {
        QJsonObject feat = f.toObject();
        QJsonObject props = feat["properties"].toObject();
        QJsonArray center = feat["center"].toArray();

        PlaceResult r;
        r.name = feat["text"].toString();
        r.address = feat["place_name"].toString();
        if (center.size() >= 2) {
            r.lon = center[0].toDouble();
            r.lat = center[1].toDouble();
            r.distanceKm = haversineDistance(myLat, myLon, r.lat, r.lon);
        }
        // Extract category from context array
        QJsonArray context = feat["context"].toArray();
        r.category = props["category"].toString();

        results.append(r);
    }

    // Try Google Places enrichment for ratings
    if (!m_googleApiKey.isEmpty() && !results.isEmpty()) {
        enrichWithGoogle(results);
    } else {
        emit searchCompleted(formatResults(results));
    }
}

void PlacesSearchManager::enrichWithGoogle(QList<PlaceResult> results)
{
    m_pendingResults = results;
    m_pendingGoogleRequests = results.size();

    for (int i = 0; i < results.size(); ++i) {
        const auto &r = results[i];
        QString url = "https://maps.googleapis.com/maps/api/place/findplacefromtext/json";
        QUrlQuery params;
        params.addQueryItem("key", m_googleApiKey);
        params.addQueryItem("input", r.name);
        params.addQueryItem("inputtype", "textquery");
        params.addQueryItem("fields", "rating,opening_hours");
        params.addQueryItem("locationbias", QString("circle:5000@%1,%2").arg(r.lat, 0, 'f', 6).arg(r.lon, 0, 'f', 6));

        QUrl requestUrl(url);
        requestUrl.setQuery(params);

        QNetworkRequest req(requestUrl);
        req.setAttribute(QNetworkRequest::User, i);  // Store index
        m_googleNetwork->get(req);
    }
}

void PlacesSearchManager::onGoogleReply(QNetworkReply *reply)
{
    reply->deleteLater();

    int idx = reply->request().attribute(QNetworkRequest::User).toInt();

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray candidates = doc.object()["candidates"].toArray();
        if (!candidates.isEmpty()) {
            QJsonObject place = candidates[0].toObject();
            if (place.contains("rating") && idx < m_pendingResults.size()) {
                m_pendingResults[idx].rating = place["rating"].toDouble();
                m_pendingResults[idx].hasRating = true;
            }
        }
    }

    m_pendingGoogleRequests--;
    if (m_pendingGoogleRequests <= 0) {
        emit searchCompleted(formatResults(m_pendingResults));
        m_pendingResults.clear();
    }
}

QString PlacesSearchManager::formatResults(const QList<PlaceResult> &results) const
{
    QString out;
    for (int i = 0; i < results.size(); ++i) {
        const auto &r = results[i];
        out += QString("%1. %2").arg(i + 1).arg(r.name);
        if (r.distanceKm < 1.0) {
            out += QString(" (%1 m away)").arg(qRound(r.distanceKm * 1000));
        } else {
            out += QString(" (%1 km away)").arg(r.distanceKm, 0, 'f', 1);
        }
        if (r.hasRating) {
            out += QString(", rated %1/5").arg(r.rating, 0, 'f', 1);
        }
        if (!r.address.isEmpty()) {
            // Extract just the street address (before first comma after name)
            QString addr = r.address;
            int nameEnd = addr.indexOf(r.name);
            if (nameEnd >= 0) {
                addr = addr.mid(nameEnd + r.name.length());
                if (addr.startsWith(", ")) addr = addr.mid(2);
            }
            // Take first part only
            int comma = addr.indexOf(',');
            if (comma > 0) addr = addr.left(comma);
            if (!addr.isEmpty()) out += QString(", %1").arg(addr);
        }
        out += "\n";
    }
    return out;
}

void PlacesSearchManager::geocodePlace(const QString &query)
{
    m_lastGeocodeQuery = query;
    double lat = m_context ? m_context->gpsLatitude() : 0.0;
    double lon = m_context ? m_context->gpsLongitude() : 0.0;

    qDebug() << "PlacesSearchManager: Geocoding place:" << query << "near" << lat << lon;

    // OSM Nominatim — community-edited POI coordinates, most accurate building positions
    QUrl url("https://nominatim.openstreetmap.org/search");
    QUrlQuery params;
    params.addQueryItem("q", query);
    params.addQueryItem("format", "json");
    params.addQueryItem("limit", "10");
    params.addQueryItem("countrycodes", "ca");
    params.addQueryItem("addressdetails", "1");
    url.setQuery(params);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "HeadUnit/1.0");
    m_geocodeNetwork->get(req);
}

void PlacesSearchManager::onGeocodeReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "PlacesSearchManager: Geocode failed:" << reply->errorString();
        emit searchFailed("Place search failed: " + reply->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray results = doc.array();

    if (results.isEmpty()) {
        qDebug() << "PlacesSearchManager: No geocode results";
        emit searchFailed("Place not found");
        return;
    }

    // Score results against query words
    int bestIdx = 0;
    QString queryLower = m_lastGeocodeQuery.toLower();

    static const QStringList skipWords = {"in", "the", "to", "at", "on", "near", "a", "an"};
    QStringList queryWords;
    for (const QString &word : queryLower.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts)) {
        if (!skipWords.contains(word) && word.length() > 2) {
            queryWords.append(word);
        }
    }

    int bestScore = -1;
    for (int i = 0; i < results.size(); ++i) {
        QJsonObject r = results[i].toObject();
        QString name = r["name"].toString().toLower();
        QString displayName = r["display_name"].toString().toLower();

        int score = 0;
        for (const QString &word : queryWords) {
            if (name.contains(word)) score += 2;
            else if (displayName.contains(word)) score += 1;
        }
        qDebug() << "PlacesSearchManager: Result" << i << r["name"].toString()
                 << "score:" << score << "at" << r["lat"].toString() << r["lon"].toString()
                 << r["display_name"].toString().left(80);
        if (score > bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }

    QJsonObject best = results[bestIdx].toObject();
    double lat = best["lat"].toString().toDouble();
    double lon = best["lon"].toString().toDouble();
    QString name = best["name"].toString();
    QString displayName = best["display_name"].toString();

    // If Nominatim name is empty, extract from display_name
    if (name.isEmpty()) {
        name = displayName.section(',', 0, 0).trimmed();
    }

    qDebug() << "PlacesSearchManager: Geocoded" << name << "at" << lat << lon << displayName.left(80);
    emit geocodeCompleted(lat, lon, name);
}

double PlacesSearchManager::haversineDistance(double lat1, double lon1, double lat2, double lon2) const
{
    const double R = 6371.0; // Earth radius km
    double dLat = qDegreesToRadians(lat2 - lat1);
    double dLon = qDegreesToRadians(lon2 - lon1);
    double a = qSin(dLat / 2) * qSin(dLat / 2)
             + qCos(qDegreesToRadians(lat1)) * qCos(qDegreesToRadians(lat2))
             * qSin(dLon / 2) * qSin(dLon / 2);
    double c = 2 * qAtan2(qSqrt(a), qSqrt(1 - a));
    return R * c;
}
