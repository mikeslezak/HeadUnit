#ifndef GEOUTILS_H
#define GEOUTILS_H

#include <QtMath>
#include <QJsonArray>
#include <QString>

namespace GeoUtils {

inline double haversineKm(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371.0;
    double dLat = qDegreesToRadians(lat2 - lat1);
    double dLon = qDegreesToRadians(lon2 - lon1);
    double a = qSin(dLat / 2) * qSin(dLat / 2)
             + qCos(qDegreesToRadians(lat1)) * qCos(qDegreesToRadians(lat2))
             * qSin(dLon / 2) * qSin(dLon / 2);
    return R * 2 * qAtan2(qSqrt(a), qSqrt(1 - a));
}

// Distance from point P to the closest point on line segment AB, in km.
// Uses flat-earth approximation (accurate for short segments).
inline double pointToSegmentDistanceKm(double pLat, double pLon,
                                        double aLat, double aLon,
                                        double bLat, double bLon)
{
    double dx = bLon - aLon;
    double dy = bLat - aLat;
    double lenSq = dx * dx + dy * dy;

    if (lenSq < 1e-12) {
        // A and B are the same point
        return haversineKm(pLat, pLon, aLat, aLon);
    }

    // Parameter t of the projection of P onto AB, clamped to [0,1]
    double t = ((pLon - aLon) * dx + (pLat - aLat) * dy) / lenSq;
    t = qBound(0.0, t, 1.0);

    // Closest point on segment
    double closestLat = aLat + t * dy;
    double closestLon = aLon + t * dx;

    return haversineKm(pLat, pLon, closestLat, closestLon);
}

// Douglas-Peucker polyline simplification.
// Input: QJsonArray of GeoJSON [lon, lat] pairs.
// Returns simplified array with <= targetPoints points.
inline QJsonArray simplifyRoute(const QJsonArray &coords, int targetPoints = 100)
{
    if (coords.size() <= targetPoints) return coords;

    // Recursive Douglas-Peucker implementation
    struct DP {
        static void run(const QJsonArray &pts, int first, int last,
                        double epsilon, QVector<bool> &keep)
        {
            if (last <= first + 1) return;

            double aLat = pts[first].toArray()[1].toDouble();
            double aLon = pts[first].toArray()[0].toDouble();
            double bLat = pts[last].toArray()[1].toDouble();
            double bLon = pts[last].toArray()[0].toDouble();

            double maxDist = 0.0;
            int maxIdx = first;

            for (int i = first + 1; i < last; ++i) {
                double pLat = pts[i].toArray()[1].toDouble();
                double pLon = pts[i].toArray()[0].toDouble();
                double d = pointToSegmentDistanceKm(pLat, pLon, aLat, aLon, bLat, bLon);
                if (d > maxDist) {
                    maxDist = d;
                    maxIdx = i;
                }
            }

            if (maxDist > epsilon) {
                keep[maxIdx] = true;
                run(pts, first, maxIdx, epsilon, keep);
                run(pts, maxIdx, last, epsilon, keep);
            }
        }
    };

    // Auto-tune epsilon: start at 0.05 km, double until we hit target
    double epsilon = 0.05;
    QJsonArray result;

    for (int attempt = 0; attempt < 20; ++attempt) {
        QVector<bool> keep(coords.size(), false);
        keep[0] = true;
        keep[coords.size() - 1] = true;

        DP::run(coords, 0, coords.size() - 1, epsilon, keep);

        int count = 0;
        for (bool k : keep) { if (k) ++count; }

        if (count <= targetPoints || attempt == 19) {
            result = QJsonArray();
            for (int i = 0; i < coords.size(); ++i) {
                if (keep[i]) result.append(coords[i]);
            }
            break;
        }

        epsilon *= 2.0;
    }

    return result;
}

// Google Encoded Polyline encoder.
// Input: QJsonArray of GeoJSON [lon, lat] pairs.
// Output: encoded polyline QString per Google's algorithm.
inline QString encodePolyline(const QJsonArray &coords)
{
    QString result;
    int prevLat = 0, prevLon = 0;

    for (int i = 0; i < coords.size(); ++i) {
        QJsonArray pt = coords[i].toArray();
        double lon = pt[0].toDouble();
        double lat = pt[1].toDouble();

        int iLat = qRound(lat * 1e5);
        int iLon = qRound(lon * 1e5);

        int dLat = iLat - prevLat;
        int dLon = iLon - prevLon;
        prevLat = iLat;
        prevLon = iLon;

        auto encodeValue = [&result](int v) {
            // Zigzag encode: left-shift 1, XOR with arithmetic right-shift 31
            v = (v < 0) ? ~(v << 1) : (v << 1);
            // Break into 5-bit chunks with 0x20 continuation bit, add 63
            while (v >= 0x20) {
                result += QChar((0x20 | (v & 0x1F)) + 63);
                v >>= 5;
            }
            result += QChar(v + 63);
        };

        encodeValue(dLat);
        encodeValue(dLon);
    }

    return result;
}

} // namespace GeoUtils
#endif
