#pragma once

#include "PackingTypes.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace wk::AtlasGenerator::packing {
    class EdgeCache {
    public:
        explicit EdgeCache(const Path& path, double accuracy = 1.0) :
            m_accuracy(accuracy) {
            build(path, m_contour);
        }

        explicit EdgeCache(const Polygon& polygon, double accuracy = 1.0) :
            m_accuracy(accuracy) {
            build(polygon.contour, m_contour);
            m_holes.reserve(polygon.holes.size());
            for (const Path& hole : polygon.holes) {
                m_holes.emplace_back();
                build(hole, m_holes.back());
            }
        }

        double circumference() const { return m_contour.full_distance; }
        double circumference(size_t hole_index) const { return m_holes[hole_index].full_distance; }

        const std::vector<double>& corners() const {
            fetch_corners(m_contour);
            return m_contour.corners;
        }

        const std::vector<double>& corners(size_t hole_index) const {
            ContourCache& hole = m_holes[hole_index];
            fetch_hole_corners(hole);
            return hole.corners;
        }

        size_t hole_count() const { return m_holes.size(); }

        Point coords(double distance) const { return coords(m_contour, distance); }

        Point coords(size_t hole_index, double distance) const { return coords(m_holes[hole_index], distance); }

    private:
        struct Edge {
            Point first;
            Point second;
            double cos_angle = 0.0;
            double sin_angle = 0.0;
        };

        struct ContourCache {
            mutable std::vector<double> corners;
            std::vector<Edge> edges;
            std::vector<double> distances;
            double full_distance = 0.0;
        };

        void build(const Path& path, ContourCache& cache) {
            if (path.size() < 2)
                return;

            cache.edges.reserve(path.size());
            cache.distances.reserve(path.size());

            for (size_t i = 0; i + 1 < path.size(); ++i) {
                const double dx = static_cast<double>(path[i + 1].X - path[i].X);
                const double dy = static_cast<double>(path[i + 1].Y - path[i].Y);
                cache.full_distance += std::sqrt(dx * dx + dy * dy);

                double angle = std::atan2(dy, dx);
                if (std::signbit(angle))
                    angle += 2.0 * std::numbers::pi_v<double>;
                cache.edges.push_back({path[i], path[i + 1], std::cos(angle), std::sin(angle)});
                cache.distances.push_back(cache.full_distance);
            }
        }

        size_t stride(size_t count) const {
            const double exponent = std::pow(m_accuracy, 1.0 / 3.0);
            const double value =
                std::round(static_cast<double>(count) / std::pow(static_cast<double>(count), exponent));
            const auto result = static_cast<Coord>(value);
            return result > 0 ? static_cast<size_t>(result) : 1;
        }

        void fetch_corners(ContourCache& cache) const {
            if (!cache.corners.empty())
                return;

            const size_t count = cache.distances.size();
            if (count == 0 || cache.full_distance <= 0.0)
                return;
            const size_t step = stride(count);

            cache.corners.reserve(count / step + 1);
            cache.corners.emplace_back(0.0);
            const size_t last = count - 1;
            cache.corners.emplace_back(0.0);
            for (size_t i = 0; i < last; i += step) {
                cache.corners.emplace_back(cache.distances[i] / cache.full_distance);
            }
        }

        void fetch_hole_corners(ContourCache& cache) const {
            if (!cache.corners.empty())
                return;

            const size_t count = cache.distances.size();
            if (count == 0 || cache.full_distance <= 0.0)
                return;
            const size_t last = count - 1;
            const size_t step = stride(count);

            cache.corners.reserve(count / step + 1);
            cache.corners.emplace_back(0.0);
            for (size_t i = 0; i < last; i += step) {
                cache.corners.emplace_back(cache.distances[i] / cache.full_distance);
            }
        }

        Point coords(const ContourCache& cache, double distance) const {
            if (cache.distances.empty() || cache.edges.empty())
                return {0, 0};
            if (distance > 1.0)
                distance = std::fmod(distance, 1.0);
            if (distance < 0.0)
                distance = 0.0;

            const double d = distance * cache.full_distance;
            auto it = std::lower_bound(cache.distances.begin(), cache.distances.end(), d);
            size_t index = static_cast<size_t>(it - cache.distances.begin());
            if (index >= cache.edges.size())
                index = cache.edges.size() - 1;

            const double previous = index == 0 ? 0.0 : cache.distances[index - 1];
            const double edge_distance = d - previous;
            const Edge& edge = cache.edges[index];

            return {edge.first.X + static_cast<Coord>(std::round(edge_distance * edge.cos_angle)),
                    edge.first.Y + static_cast<Coord>(std::round(edge_distance * edge.sin_angle))};
        }

        mutable ContourCache m_contour;
        mutable std::vector<ContourCache> m_holes;
        double m_accuracy = 1.0;
    };
}
