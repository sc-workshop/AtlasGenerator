#pragma once

#include "PackingTypes.h"
#include "core/preprocessor/api.h"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace wk::AtlasGenerator::packing {
    WK_INLINE const std::vector<double>& default_rotations() {
        static const std::vector<double> rotations {0.0,
                                                    std::numbers::pi_v<double> / 2.0,
                                                    std::numbers::pi_v<double>,
                                                    3.0 * std::numbers::pi_v<double> / 2.0};
        return rotations;
    }

    WK_INLINE std::vector<double> optimized_rotations(const PackingItem& item) {
        const Path& shape = item.shape();
        const int count = static_cast<int>(shape.size()) - 1;
        if (count < 2)
            return {0.0};

        bool is_straight = true;
        size_t poly_distance = 0;
        for (int i = 0; i < count; ++i) {
            const Point& p = shape[static_cast<size_t>(i)];
            const Point& q = shape[static_cast<size_t>((i + 1) % count)];
            const double dx = static_cast<double>(p.X - q.X);
            const double dy = static_cast<double>(p.Y - q.Y);
            const float distance = static_cast<float>(std::sqrt(std::pow(dx, 2) + std::pow(dy, 2)));
            const size_t truncated = static_cast<size_t>(distance);

            if (i != 0) {
                if (truncated != poly_distance) {
                    is_straight = false;
                    break;
                }
            } else {
                poly_distance = truncated;
            }
        }

        if (is_straight)
            return {0.0};

        int32_t cx = 0;
        int32_t cy = 0;
        for (const Point& point : shape) {
            cx += static_cast<int32_t>(point.X);
            cy += static_cast<int32_t>(point.Y);
        }
        const Point centroid {static_cast<int>(std::ceil(static_cast<double>(cx / count))),
                              static_cast<int>(std::ceil(static_cast<double>(cy / count)))};

        bool symmetry_x = true;
        for (int i = 0; i < count && symmetry_x; ++i) {
            const Point& p = shape[static_cast<size_t>(i)];
            bool has_mirror = false;
            for (int t = 0; t < count; ++t) {
                if (i == t)
                    continue;
                const Point& q = shape[static_cast<size_t>(t)];
                if (p.X == q.X && std::abs(p.Y + q.Y - 2 * centroid.Y) == 0) {
                    has_mirror = true;
                    break;
                }
            }
            if (!has_mirror)
                symmetry_x = false;
        }

        bool symmetry_y = true;
        for (int i = 0; i < count && symmetry_y; ++i) {
            const Point& p = shape[static_cast<size_t>(i)];
            bool has_mirror = false;
            for (int t = 0; t < count; ++t) {
                if (i == t)
                    continue;
                const Point& q = shape[static_cast<size_t>(t)];
                if (std::abs(p.X + q.X - 2 * centroid.X) == 0 && p.Y == q.Y) {
                    has_mirror = true;
                    break;
                }
            }
            if (!has_mirror)
                symmetry_y = false;
        }

        std::vector<double> result {0.0};
        result.reserve(4);
        if (!symmetry_x)
            result.push_back(std::numbers::pi_v<double>);
        if (!symmetry_y) {
            result.push_back(std::numbers::pi_v<double> / 2.0);
            result.push_back(3.0 * std::numbers::pi_v<double> / 2.0);
        }
        return result;
    }
}
