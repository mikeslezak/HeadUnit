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

    Q_INVOKABLE void searchPlaces(const QString &query, const QString &category = QString());
    Q_INVOKABLE void geocodePlace(const QString &query);

signals:
    void searchCompleted(const QString &formattedResults);
    void searchFailed(const QString &error);
    void geocodeCompleted(double lat, double lon, const QString &name);

private slots:
    void onMapboxReply(QNetworkReply *reply);
    void onGoogleReply(QNetworkReply *reply);
    void onGeocodeReply(QNetworkReply *reply);

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
    };

    QString formatResults(const QList<PlaceResult> &results) const;
    void enrichWithGoogle(QList<PlaceResult> results);
    double haversineDistance(double lat1, double lon1, double lat2, double lon2) const;

    QNetworkAccessManager *m_mapboxNetwork;
    QNetworkAccessManager *m_googleNetwork;
    QNetworkAccessManager *m_geocodeNetwork;
    QString m_mapboxToken;
    QString m_googleApiKey;
    QString m_lastGeocodeQuery;
    ContextAggregator *m_context = nullptr;

    QList<PlaceResult> m_pendingResults;
    int m_pendingGoogleRequests = 0;
};

#endif // PLACESSEARCHMANAGER_H
