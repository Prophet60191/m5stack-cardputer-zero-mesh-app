// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace viewmodel::geo {
inline double to_rad(double degrees) { return degrees * 3.14159265358979323846 / 180.0; }
inline double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    const double slat = std::sin(to_rad(lat2 - lat1) / 2), slon = std::sin(to_rad(lon2 - lon1) / 2);
    const double a = slat * slat + std::cos(to_rad(lat1)) * std::cos(to_rad(lat2)) * slon * slon;
    const double bounded = std::clamp(a, 0.0, 1.0);
    return 6371.0 * 2 * std::atan2(std::sqrt(bounded), std::sqrt(1 - bounded));
}
inline std::string distance_text(double km) {
    if (!std::isfinite(km) || km < 0)
        return "unknown";
    char text[32];
    if (km < 1)
        std::snprintf(text, sizeof(text), "%d m", static_cast<int>(km * 1000));
    else
        std::snprintf(text, sizeof(text), "%.1f km", km);
    return text;
}
inline bool fresh(long stamp, long now) { return stamp > 0 && now >= stamp && now - stamp <= 120; }
} // namespace viewmodel::geo
