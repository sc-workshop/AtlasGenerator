#pragma once

#include "PackingTypes.h"
#include "core/algorithm/sort.hpp"
#include "core/preprocessor/api.h"

#include <algorithm>
#include <array>
#include <vector>

namespace wk::AtlasGenerator::packing {
    struct NfpResult {
        Path path;
        Point reference {0, 0};
    };

    namespace detail {

        struct Edge {
            Point first;
            Point second;
        };

        WK_INLINE bool edge_less(const Edge& a, const Edge& b) {
            const Point ax {1, 0};
            const Point p1 {a.second.X - a.first.X, a.second.Y - a.first.Y};
            const Point p2 {b.second.X - b.first.X, b.second.Y - b.first.Y};

            const std::array<int, 4> quadrants {0, 3, 1, 2};
            std::array<int, 2> q {0, 0};
            const std::array<long long, 2> lcos {static_cast<long long>(p1.X), static_cast<long long>(p2.X)};
            const std::array<long long, 2> lsin {static_cast<long long>(p1.Y), static_cast<long long>(p2.Y)};

            for (size_t i = 0; i < 2; ++i) {
                if (lcos[i] == 0)
                    q[i] = lsin[i] > 0 ? 1 : 3;
                else if (lsin[i] == 0)
                    q[i] = lcos[i] > 0 ? 0 : 2;
                else
                    q[i] = quadrants[static_cast<size_t>((lcos[i] < 0) << 1) + static_cast<size_t>(lsin[i] < 0)];
            }

            if (q[0] == q[1]) {
                const std::array<long long, 2> lsq {static_cast<long long>(p1.X) * p1.X +
                                                        static_cast<long long>(p1.Y) * p1.Y,
                                                    static_cast<long long>(p2.X) * p2.X +
                                                        static_cast<long long>(p2.Y) * p2.Y};
                const int sign = q[0] == 1 || q[0] == 2 ? -1 : 1;
                const double pcos1 =
                    static_cast<double>(lcos[0]) / static_cast<double>(lsq[0]) * sign * static_cast<double>(lcos[0]);
                const double pcos2 =
                    static_cast<double>(lcos[1]) / static_cast<double>(lsq[1]) * sign * static_cast<double>(lcos[1]);
                return q[0] < 2 ? pcos1 < pcos2 : pcos1 > pcos2;
            }

            (void) ax;
            return q[0] > q[1];
        }

        WK_INLINE NfpResult build_polygon(const std::vector<Edge>& edges) {
            NfpResult result;
            if (edges.empty())
                return result;

            result.path.reserve(edges.size());
            result.path.emplace_back(edges.front().first);
            result.path.emplace_back(edges.front().second);
            result.reference = *std::max_element(result.path.begin(), result.path.end(), vertex_less);

            size_t previous_index = 1;
            for (size_t i = 1; i < edges.size(); ++i) {
                const Point delta {result.path[previous_index].X - edges[i].first.X,
                                   result.path[previous_index].Y - edges[i].first.Y};
                const Point point {edges[i].second.X + delta.X, edges[i].second.Y + delta.Y};
                result.path.emplace_back(point);
                if (vertex_less(result.reference, point))
                    result.reference = point;
                ++previous_index;
            }

            return result;
        }

    }

    WK_INLINE NfpResult convex_nfp(const Path& stationary, const Path& orbiter) {
        std::vector<detail::Edge> edges;
        edges.reserve(stationary.size() + orbiter.size());

        for (size_t i = 0; i + 1 < stationary.size(); ++i) {
            edges.push_back({stationary[i], stationary[i + 1]});
        }
        for (size_t i = 0; i + 1 < orbiter.size(); ++i) {
            edges.push_back({orbiter[i + 1], orbiter[i]});
        }

        if (edges.empty())
            return {};
        std::sort(edges.begin(), edges.end(), detail::edge_less);
        return detail::build_polygon(edges);
    }

    WK_INLINE void correct_nfp_position(NfpResult& nfp, const PackingItem& stationary, const PackingItem& orbiter) {
        const Point touch_stationary = stationary.rightmost_top_vertex();
        const Point touch_orbiter = orbiter.leftmost_bottom_vertex();
        const Point dtouch {touch_stationary.X - touch_orbiter.X, touch_stationary.Y - touch_orbiter.Y};
        const Point top_orbiter = orbiter.rightmost_top_vertex() + dtouch;
        const Point dnfp {top_orbiter.X - nfp.reference.X, top_orbiter.Y - nfp.reference.Y};
        translate_path(nfp.path, dnfp);
    }
}
