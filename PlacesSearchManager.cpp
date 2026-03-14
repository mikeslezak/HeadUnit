#include "PlacesSearchManager.h"
#include "ContextAggregator.h"
#include "GeoUtils.h"
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
    , m_placesNewNetwork(new QNetworkAccessManager(this))
{
    connect(m_mapboxNetwork, &QNetworkAccessManager::finished,
            this, &PlacesSearchManager::onMapboxReply);
    connect(m_googleNetwork, &QNetworkAccessManager::finished,
            this, &PlacesSearchManager::onGoogleReply);
    connect(m_geocodeNetwork, &QNetworkAccessManager::finished,
            this, &PlacesSearchManager::onGeocodeReply);
    connect(m_placesNewNetwork, &QNetworkAccessManager::finished,
            this, &PlacesSearchManager::onSearchTextReply);
    qDebug() << "PlacesSearchManager: Initialized";
}

void PlacesSearchManager::setMapboxToken(const QString &token) { m_mapboxToken = token; }
void PlacesSearchManager::setGoogleApiKey(const QString &key) { m_googleApiKey = key; }
void PlacesSearchManager::setContextAggregator(ContextAggregator *ctx) { m_context = ctx; }

void PlacesSearchManager::searchPlaces(const QString &query, const QString &category,
                                        bool alongRoute, const QString &near)
{
    ++m_generation;
    m_currentQuery = query;

    if (m_googleApiKey.isEmpty()) {
        emit searchFailed("Google API key not configured");
        return;
    }

    double lat = m_context ? m_context->bestLatitude() : 0.0;
    double lon = m_context ? m_context->bestLongitude() : 0.0;

    // Path A — `near` provided: geocode first, then search
    if (!near.isEmpty()) {
        qDebug() << "PlacesSearchManager: Deferred search — geocoding" << near << "first";
        m_deferredQuery = query;
        m_deferredCategory = category;
        m_deferredAlongRoute = alongRoute;
        int gen = m_generation;

        // One-shot connections for geocode result
        QMetaObject::Connection *successConn = new QMetaObject::Connection();
        QMetaObject::Connection *failConn = new QMetaObject::Connection();

        *successConn = connect(this, &PlacesSearchManager::geocodeCompleted, this,
            [this, gen, successConn, failConn](double gLat, double gLon, const QString &) {
                disconnect(*successConn);
                disconnect(*failConn);
                delete successConn;
                delete failConn;
                if (gen != m_generation) return; // Stale
                executeDeferredSearch(gLat, gLon);
            });

        *failConn = connect(this, &PlacesSearchManager::geocodeFailed, this,
            [this, gen, successConn, failConn](const QString &error) {
                disconnect(*successConn);
                disconnect(*failConn);
                delete successConn;
                delete failConn;
                if (gen != m_generation) return; // Stale
                emit searchFailed("Could not find location: " + error);
            });

        geocodePlace(near);
        return;
    }

    if (lat == 0.0 && lon == 0.0) {
        emit searchFailed("No GPS location available for nearby search");
        return;
    }

    // Path B — along-route with full polyline (new API)
    if (alongRoute && m_context && !m_context->routeCoordinates().isEmpty()) {
        searchAlongRouteNew(query, category, lat, lon);
        return;
    }

    // Path C — normal single-point search
    qDebug() << "PlacesSearchManager: Searching for" << query << "near" << lat << lon;
    searchAtPoint(lat, lon, query, category, 8000);
}

void PlacesSearchManager::searchAtPoint(double lat, double lon, const QString &query, const QString &category, int radiusM)
{
    QString url = "https://maps.googleapis.com/maps/api/place/nearbysearch/json";

    QUrlQuery params;
    params.addQueryItem("key", m_googleApiKey);
    params.addQueryItem("location", QString("%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
    params.addQueryItem("radius", QString::number(radiusM));
    params.addQueryItem("keyword", query);
    if (!category.isEmpty()) {
        params.addQueryItem("type", category);
    }

    QUrl requestUrl(url);
    requestUrl.setQuery(params);

    QNetworkRequest req(requestUrl);
    req.setTransferTimeout(15000);
    req.setAttribute(QNetworkRequest::UserMax, m_generation);
    m_mapboxNetwork->get(req);
}

void PlacesSearchManager::onMapboxReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (m_pendingRouteSearches > 0) {
            m_pendingRouteSearches--;
            m_failedRouteSearches++;
            if (m_pendingRouteSearches <= 0) collectAlongRouteResults();
        } else {
            emit searchFailed("Search failed: " + reply->errorString());
        }
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    QString status = root["status"].toString();

    QList<PlaceResult> results;

    if (status == "OK") {
        QJsonArray resultsArray = root["results"].toArray();
        double myLat = m_context ? m_context->bestLatitude() : 0.0;
        double myLon = m_context ? m_context->bestLongitude() : 0.0;

        int limit = qMin(5, resultsArray.size());
        for (int i = 0; i < limit; ++i) {
            QJsonObject place = resultsArray[i].toObject();
            QJsonObject geometry = place["geometry"].toObject();
            QJsonObject location = geometry["location"].toObject();

            PlaceResult r;
            r.name = place["name"].toString();
            r.address = place["vicinity"].toString();
            r.lat = location["lat"].toDouble();
            r.lon = location["lng"].toDouble();
            r.distanceKm = GeoUtils::haversineKm(myLat, myLon, r.lat, r.lon);

            if (place.contains("rating")) {
                r.rating = place["rating"].toDouble();
                r.hasRating = true;
            }

            results.append(r);
        }
    }

    // Along-route mode: accumulate results from multiple search points
    if (m_pendingRouteSearches > 0) {
        // Deduplicate by name
        for (const auto &r : results) {
            bool duplicate = false;
            for (const auto &existing : m_routeSearchResults) {
                if (existing.name == r.name) { duplicate = true; break; }
            }
            if (!duplicate) m_routeSearchResults.append(r);
        }
        m_pendingRouteSearches--;
        if (m_pendingRouteSearches <= 0) {
            collectAlongRouteResults();
        }
        return;
    }

    // Normal single-point search
    if (results.isEmpty()) {
        emit searchCompleted("No results found nearby.");
        return;
    }

    qDebug() << "PlacesSearchManager:" << results.size() << "results found";
    emit searchCompleted(formatResults(results));
}

void PlacesSearchManager::collectAlongRouteResults()
{
    if (m_routeSearchResults.isEmpty()) {
        // If ALL sub-searches failed with network errors, report as error
        if (m_failedRouteSearches > 0) {
            emit searchFailed("Search failed: along-route queries returned errors");
        } else {
            emit searchCompleted("No results found along the route.");
        }
        return;
    }

    // Sort by distance from current location
    std::sort(m_routeSearchResults.begin(), m_routeSearchResults.end(),
              [](const PlaceResult &a, const PlaceResult &b) {
                  return a.distanceKm < b.distanceKm;
              });

    // Take top 6 results
    QList<PlaceResult> top;
    int limit = qMin(6, m_routeSearchResults.size());
    for (int i = 0; i < limit; ++i) {
        top.append(m_routeSearchResults[i]);
    }

    qDebug() << "PlacesSearchManager: Along-route search found" << m_routeSearchResults.size()
             << "total, returning top" << top.size();
    m_routeSearchResults.clear();
    emit searchCompleted(formatResults(top));
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
        req.setAttribute(QNetworkRequest::UserMax, m_generation);
        req.setTransferTimeout(15000);
        m_googleNetwork->get(req);
    }
}

void PlacesSearchManager::onGoogleReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

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

        // Show detour info if available, otherwise straight-line distance
        if (r.detourDurationMin >= 0) {
            int mins = qRound(r.detourDurationMin);
            if (r.detourDistanceKm >= 0) {
                out += QString(" (%1 min detour, %2 km off route)")
                           .arg(mins)
                           .arg(r.detourDistanceKm, 0, 'f', 1);
            } else {
                out += QString(" (%1 min detour)").arg(mins);
            }
        } else if (r.distanceKm < 1.0) {
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

void PlacesSearchManager::executeDeferredSearch(double lat, double lon)
{
    qDebug() << "PlacesSearchManager: Executing deferred search at" << lat << lon
             << "query:" << m_deferredQuery << "alongRoute:" << m_deferredAlongRoute;

    if (m_deferredAlongRoute && m_context && !m_context->routeCoordinates().isEmpty()) {
        searchAlongRouteNew(m_deferredQuery, m_deferredCategory, lat, lon);
    } else {
        searchAtPoint(lat, lon, m_deferredQuery, m_deferredCategory, 15000);
    }
}

void PlacesSearchManager::searchAlongRouteNew(const QString &query, const QString &category,
                                               double originLat, double originLon)
{
    QJsonArray coords = m_context->routeCoordinates();
    QJsonArray simplified = GeoUtils::simplifyRoute(coords, 100);
    QString encoded = GeoUtils::encodePolyline(simplified);

    if (encoded.isEmpty()) {
        qWarning() << "PlacesSearchManager: Polyline encoding failed, falling back to old method";
        fallbackAlongRouteOld(query, category);
        return;
    }

    qDebug() << "PlacesSearchManager: Along-route Text Search for" << query
             << "polyline:" << simplified.size() << "pts (from" << coords.size() << ")";

    // Build the text query — append category if provided
    QString textQuery = query;
    if (!category.isEmpty() && !query.contains(category, Qt::CaseInsensitive)) {
        textQuery += " " + category;
    }

    // Build POST body
    QJsonObject body;
    body["textQuery"] = textQuery;
    body["maxResultCount"] = 10;
    body["languageCode"] = "en";

    QJsonObject polylineObj;
    polylineObj["encodedPolyline"] = encoded;
    QJsonObject searchAlongRoute;
    searchAlongRoute["polyline"] = polylineObj;
    body["searchAlongRouteParameters"] = searchAlongRoute;

    QJsonObject origin;
    origin["latitude"] = originLat;
    origin["longitude"] = originLon;
    QJsonObject routingParams;
    routingParams["origin"] = origin;
    body["routingParameters"] = routingParams;

    QUrl url("https://places.googleapis.com/v1/places:searchText");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Goog-Api-Key", m_googleApiKey.toUtf8());
    req.setRawHeader("X-Goog-FieldMask",
        "places.displayName,places.location,places.formattedAddress,places.rating,"
        "places.userRatingCount,routingSummaries");
    req.setAttribute(QNetworkRequest::UserMax, m_generation);
    req.setTransferTimeout(15000);

    m_placesNewNetwork->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void PlacesSearchManager::onSearchTextReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_generation) {
        return;
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
        qWarning() << "PlacesSearchManager: Text Search (New) failed — HTTP"
                   << statusCode << reply->errorString();
        qWarning() << "PlacesSearchManager: Response:" << reply->readAll().left(500);
        fallbackAlongRouteOld(m_currentQuery, QString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    QJsonArray places = root["places"].toArray();
    QJsonArray routingSummaries = root["routingSummaries"].toArray();

    double myLat = m_context ? m_context->bestLatitude() : 0.0;
    double myLon = m_context ? m_context->bestLongitude() : 0.0;

    QList<PlaceResult> results;
    for (int i = 0; i < places.size(); ++i) {
        QJsonObject place = places[i].toObject();
        QJsonObject location = place["location"].toObject();

        PlaceResult r;
        r.name = place["displayName"].toObject()["text"].toString();
        r.address = place["formattedAddress"].toString();
        r.lat = location["latitude"].toDouble();
        r.lon = location["longitude"].toDouble();
        r.distanceKm = GeoUtils::haversineKm(myLat, myLon, r.lat, r.lon);

        if (place.contains("rating")) {
            r.rating = place["rating"].toDouble();
            r.hasRating = true;
        }

        // Parse routing summary if available (parallel array)
        if (i < routingSummaries.size()) {
            QJsonObject summary = routingSummaries[i].toObject();
            QJsonArray legs = summary["legs"].toArray();
            if (!legs.isEmpty()) {
                QJsonObject leg = legs[0].toObject();
                if (leg.contains("distanceMeters")) {
                    r.detourDistanceKm = leg["distanceMeters"].toDouble() / 1000.0;
                }
                if (leg.contains("duration")) {
                    // Duration is in "Xs" format (e.g. "342s")
                    QString durStr = leg["duration"].toString();
                    durStr.remove('s');
                    bool ok;
                    double secs = durStr.toDouble(&ok);
                    if (ok) r.detourDurationMin = secs / 60.0;
                }
            }
        }

        results.append(r);
    }

    if (results.isEmpty()) {
        emit searchCompleted("No results found along the route.");
        return;
    }

    // Sort by detour duration (shortest first), fall back to straight-line distance
    std::sort(results.begin(), results.end(),
              [](const PlaceResult &a, const PlaceResult &b) {
                  if (a.detourDurationMin >= 0 && b.detourDurationMin >= 0)
                      return a.detourDurationMin < b.detourDurationMin;
                  if (a.detourDurationMin >= 0) return true;
                  if (b.detourDurationMin >= 0) return false;
                  return a.distanceKm < b.distanceKm;
              });

    // Cap at 6 results
    while (results.size() > 6) results.removeLast();

    qDebug() << "PlacesSearchManager: Along-route Text Search found" << places.size()
             << "results, returning top" << results.size();
    emit searchCompleted(formatResults(results));
}

void PlacesSearchManager::fallbackAlongRouteOld(const QString &query, const QString &category)
{
    qDebug() << "PlacesSearchManager: Falling back to multi-point along-route search";

    double lat = m_context ? m_context->bestLatitude() : 0.0;
    double lon = m_context ? m_context->bestLongitude() : 0.0;

    auto samplePoints = m_context->routeSamplePoints(6);
    if (samplePoints.isEmpty()) {
        // No sample points — just search at current location
        searchAtPoint(lat, lon, query, category, 15000);
        return;
    }

    m_routeSearchResults.clear();
    m_failedRouteSearches = 0;
    m_pendingRouteSearches = 1 + samplePoints.size();

    searchAtPoint(lat, lon, query, category, 15000);
    for (const auto &pt : samplePoints) {
        searchAtPoint(pt.first, pt.second, query, category, 25000);
    }
}

void PlacesSearchManager::geocodePlace(const QString &query)
{
    ++m_geocodeGeneration;
    m_lastGeocodeQuery = query;
    double lat = m_context ? m_context->bestLatitude() : 0.0;
    double lon = m_context ? m_context->bestLongitude() : 0.0;

    qDebug() << "PlacesSearchManager: Geocoding place:" << query << "near" << lat << lon;

    // OSM Nominatim — community-edited POI coordinates, most accurate building positions
    // Strip filler words that Nominatim can't handle (e.g. "GoodLife Fitness in Okotoks" -> "GoodLife Fitness Okotoks")
    static const QStringList fillerWords = {"in", "near", "at", "to", "the", "a", "an", "on", "by"};
    QStringList words = query.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QStringList cleanWords;
    for (const QString &w : words) {
        if (!fillerWords.contains(w.toLower())) {
            cleanWords.append(w);
        }
    }
    QString cleanQuery = cleanWords.join(" ");
    qDebug() << "PlacesSearchManager: Clean query:" << cleanQuery;

    QUrl url("https://nominatim.openstreetmap.org/search");
    QUrlQuery params;
    params.addQueryItem("q", cleanQuery);
    params.addQueryItem("format", "json");
    params.addQueryItem("limit", "10");
    params.addQueryItem("countrycodes", "ca,us");
    params.addQueryItem("addressdetails", "1");
    url.setQuery(params);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "HeadUnit/1.0");
    req.setAttribute(QNetworkRequest::User, "nominatim"); // Tag source
    req.setAttribute(QNetworkRequest::UserMax, m_geocodeGeneration);
    req.setTransferTimeout(15000);
    m_geocodeNetwork->get(req);
}

void PlacesSearchManager::onGeocodeReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->request().attribute(QNetworkRequest::UserMax).toInt() != m_geocodeGeneration) {
        return;
    }

    QString source = reply->request().attribute(QNetworkRequest::User).toString();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "PlacesSearchManager: Geocode failed (" << source << "):" << reply->errorString();
        emit geocodeFailed("Place search failed: " + reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (source == "nominatim") {
        QJsonArray results = doc.array();
        if (results.isEmpty()) {
            // Fallback to Google Places (New)
            qDebug() << "PlacesSearchManager: Nominatim returned no results, falling back to Google";
            geocodeFallbackGoogle();
            return;
        }
        parseNominatimResults(results);
    } else if (source == "google") {
        QJsonArray places = doc.object()["places"].toArray();
        if (places.isEmpty()) {
            qDebug() << "PlacesSearchManager: Google also returned no results";
            emit geocodeFailed("Place not found");
            return;
        }
        parseGoogleResults(places);
    }
}

void PlacesSearchManager::geocodeFallbackGoogle()
{
    if (m_googleApiKey.isEmpty()) {
        qDebug() << "PlacesSearchManager: No Google API key for fallback";
        emit geocodeFailed("Place not found");
        return;
    }

    double lat = m_context ? m_context->bestLatitude() : 0.0;
    double lon = m_context ? m_context->bestLongitude() : 0.0;

    QUrl url("https://places.googleapis.com/v1/places:searchText");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Goog-Api-Key", m_googleApiKey.toUtf8());
    req.setRawHeader("X-Goog-FieldMask",
        "places.displayName,places.location,places.formattedAddress");
    req.setAttribute(QNetworkRequest::User, "google"); // Tag source
    req.setAttribute(QNetworkRequest::UserMax, m_geocodeGeneration);
    req.setTransferTimeout(15000);

    QJsonObject body;
    body["textQuery"] = m_lastGeocodeQuery;
    body["maxResultCount"] = 5;
    body["languageCode"] = "en";

    if (lat != 0.0 || lon != 0.0) {
        QJsonObject circle;
        QJsonObject center;
        center["latitude"] = lat;
        center["longitude"] = lon;
        circle["center"] = center;
        circle["radius"] = 50000.0;
        QJsonObject locationBias;
        locationBias["circle"] = circle;
        body["locationBias"] = locationBias;
    }

    m_geocodeNetwork->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void PlacesSearchManager::parseNominatimResults(const QJsonArray &results)
{
    int bestIdx = 0;
    QString queryLower = m_lastGeocodeQuery.toLower();

    static const QStringList skipWords = {"in", "the", "to", "at", "on", "near", "a", "an"};
    QStringList queryWords;
    for (const QString &word : queryLower.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts)) {
        if (!skipWords.contains(word) && word.length() > 2) {
            queryWords.append(word);
        }
    }

    // Reference point for proximity tiebreaking: use GPS location
    double refLat = m_context ? m_context->bestLatitude() : 0.0;
    double refLon = m_context ? m_context->bestLongitude() : 0.0;
    bool hasRef = (refLat != 0.0 || refLon != 0.0);

    int bestScore = -1;
    double bestDist = 1e9;
    for (int i = 0; i < results.size(); ++i) {
        QJsonObject r = results[i].toObject();
        QString name = r["name"].toString().toLower();
        QString displayName = r["display_name"].toString().toLower();

        int score = 0;
        for (const QString &word : queryWords) {
            if (name.contains(word)) score += 2;
            else if (displayName.contains(word)) score += 1;
        }

        // Prefer Canadian results when user is in Canada (countrycodes=ca,us)
        if (displayName.contains("canada")) score += 1;

        double rLat = r["lat"].toString().toDouble();
        double rLon = r["lon"].toString().toDouble();
        double dist = hasRef ? GeoUtils::haversineKm(refLat, refLon, rLat, rLon) : 1e9;

        qDebug() << "PlacesSearchManager: Nominatim result" << i << r["name"].toString()
                 << "score:" << score << "dist:" << qRound(dist) << "km"
                 << "at" << r["lat"].toString() << r["lon"].toString();

        // Pick highest score; on tie, pick closest to reference point
        if (score > bestScore || (score == bestScore && dist < bestDist)) {
            bestScore = score;
            bestDist = dist;
            bestIdx = i;
        }
    }

    QJsonObject best = results[bestIdx].toObject();
    double lat = best["lat"].toString().toDouble();
    double lon = best["lon"].toString().toDouble();
    QString name = best["name"].toString();
    if (name.isEmpty()) {
        name = best["display_name"].toString().section(',', 0, 0).trimmed();
    }

    qDebug() << "PlacesSearchManager: Geocoded (Nominatim)" << name << "at" << lat << lon
             << "(" << qRound(bestDist) << "km from reference)";
    emit geocodeCompleted(lat, lon, name);
}

void PlacesSearchManager::parseGoogleResults(const QJsonArray &places)
{
    int bestIdx = 0;
    QString queryLower = m_lastGeocodeQuery.toLower();

    static const QStringList skipWords = {"in", "the", "to", "at", "on", "near", "a", "an"};
    QStringList queryWords;
    for (const QString &word : queryLower.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts)) {
        if (!skipWords.contains(word) && word.length() > 2) {
            queryWords.append(word);
        }
    }

    // Reference point for proximity tiebreaking
    double refLat = m_context ? m_context->bestLatitude() : 0.0;
    double refLon = m_context ? m_context->bestLongitude() : 0.0;
    bool hasRef = (refLat != 0.0 || refLon != 0.0);

    int bestScore = -1;
    double bestDist = 1e9;
    for (int i = 0; i < places.size(); ++i) {
        QJsonObject place = places[i].toObject();
        QString name = place["displayName"].toObject()["text"].toString().toLower();
        QString addr = place["formattedAddress"].toString().toLower();

        int score = 0;
        for (const QString &word : queryWords) {
            if (name.contains(word)) score += 2;
            else if (addr.contains(word)) score += 1;
        }

        // Prefer Canadian results when user is in Canada
        if (addr.contains("canada") || addr.contains(", ab,") || addr.contains(", bc,")
            || addr.contains(", ab ") || addr.contains(", bc ")) score += 1;

        QJsonObject location = place["location"].toObject();
        double pLat = location["latitude"].toDouble();
        double pLon = location["longitude"].toDouble();
        double dist = hasRef ? GeoUtils::haversineKm(refLat, refLon, pLat, pLon) : 1e9;

        qDebug() << "PlacesSearchManager: Google result" << i
                 << place["displayName"].toObject()["text"].toString()
                 << "score:" << score << "dist:" << qRound(dist) << "km";

        if (score > bestScore || (score == bestScore && dist < bestDist)) {
            bestScore = score;
            bestDist = dist;
            bestIdx = i;
        }
    }

    QJsonObject place = places[bestIdx].toObject();
    QJsonObject location = place["location"].toObject();
    double lat = location["latitude"].toDouble();
    double lon = location["longitude"].toDouble();
    QString name = place["displayName"].toObject()["text"].toString();

    qDebug() << "PlacesSearchManager: Geocoded (Google fallback)" << name << "at" << lat << lon
             << "(" << qRound(bestDist) << "km from reference)";
    emit geocodeCompleted(lat, lon, name);
}

