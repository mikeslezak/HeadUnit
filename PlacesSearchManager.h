#ifndef PLACESSEARCHMANAGER_H
#define PLACESSEARCHMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ContextAggregator;

class PlacesSearchManager : public QObject
{
    Q_OBJECT

public:
    explicit PlacesSearchManager(QObject *parent = nullptr);

    void setMapboxToken(const QString &token);
    void setGoogleApiKey(const QString &key);
    void setContextAggregator(ContextAggregator *ctx);

    Q_INVOKABLE void searchPlaces(const QString &query, const QString &category = QString(),
                                   bool alongRoute = false, const QString &near = QString());
    Q_INVOKABLE void geocodePlace(const QString &query);

signals:
    void searchCompleted(const QString &formattedResults);
    void searchFailed(const QString &error);
    void geocodeCompleted(double lat, double lon, const QString &name);
    void geocodeFailed(const QString &error);

private slots:
    void onMapboxReply(QNetworkReply *reply);
    void onGoogleReply(QNetworkReply *reply);
    void onGeocodeReply(QNetworkReply *reply);
    void onSearchTextReply(QNetworkReply *reply);

private:
    struct PlaceResult {
        QString name;
        QString address;
        double lat = 0.0;
        double lon = 0.0;
        double distanceKm = 0.0;
        QString category;
        double rating = 0.0;
        bool hasRating = false;
        double detourDistanceKm = -1.0;  // -1 = not available (from routingSummaries)
        double detourDurationMin = -1.0;  // -1 = not available
    };

    QString formatResults(const QList<PlaceResult> &results) const;
    void enrichWithGoogle(QList<PlaceResult> results);
    void geocodeFallbackGoogle();
    void parseNominatimResults(const QJsonArray &results);
    void parseGoogleResults(const QJsonArray &places);
    QNetworkAccessManager *m_mapboxNetwork;
    QNetworkAccessManager *m_googleNetwork;
    QNetworkAccessManager *m_geocodeNetwork;
    QNetworkAccessManager *m_placesNewNetwork;  // For Places Text Search (New) along-route
    QString m_mapboxToken;
    QString m_googleApiKey;
    QString m_lastGeocodeQuery;
    ContextAggregator *m_context = nullptr;

    void searchAtPoint(double lat, double lon, const QString &query, const QString &category, int radiusM);
    void collectAlongRouteResults();
    void searchAlongRouteNew(const QString &query, const QString &category,
                             double originLat, double originLon);
    void fallbackAlongRouteOld(const QString &query, const QString &category);
    void executeDeferredSearch(double lat, double lon);

    QList<PlaceResult> m_pendingResults;
    int m_pendingGoogleRequests = 0;
    int m_generation = 0;
    int m_geocodeGeneration = 0;

    // Along-route search state
    int m_pendingRouteSearches = 0;
    int m_failedRouteSearches = 0;
    QList<PlaceResult> m_routeSearchResults;
    QString m_currentQuery;

    // Deferred search state (for `near` geocode-then-search chain)
    QString m_deferredQuery;
    QString m_deferredCategory;
    bool m_deferredAlongRoute = false;
};

#endif // PLACESSEARCHMANAGER_H
