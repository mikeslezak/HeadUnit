#ifndef GEOUTILS_H
#define GEOUTILS_H

#include <QtMath>

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

} // namespace GeoUtils
#endif
