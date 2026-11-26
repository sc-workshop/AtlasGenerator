#include "PackingTypes.h"

#include <algorithm>
#include <clipper2/clipper.h>
#include <functional>
#include <iterator>

namespace wk::AtlasGenerator::packing {
    namespace {
        namespace c2 = Clipper2Lib;

        void close_path(Path& path) {
            if (path.empty())
                return;

            const Point front = path.front();
            const Point& back = path.back();
            if (front.X != back.X || front.Y != back.X) {
                path.emplace_back(front);
            }
        }

        c2::Path64 to_clipper_path(const Path& path) {
            c2::Path64 result;
            result.reserve(path.size());
            for (const Point& point : path) {
                result.emplace_back(point.X, point.Y);
            }
            return result;
        }

        Path from_clipper_path(const c2::Path64& path) {
            Path result;
            result.reserve(path.size());
            for (const c2::Point64& point : path) {
                result.emplace_back(point.x, point.y);
            }
            return result;
        }

        Shapes polytree_to_shapes(const c2::PolyTree64& tree) {
            Shapes shapes;
            shapes.reserve(tree.Count());

            std::function<void(const c2::PolyPath64*, Polygon&)> process_hole;
            std::function<void(const c2::PolyPath64*)> process_poly;

            process_hole = [&](const c2::PolyPath64* node, Polygon& polygon) {
                Path hole = from_clipper_path(node->Polygon());
                close_path(hole);
                polygon.holes.emplace_back(std::move(hole));

                for (const auto& child : *node) {
                    process_poly(child.get());
                }
            };

            process_poly = [&](const c2::PolyPath64* node) {
                Polygon polygon;
                polygon.contour = from_clipper_path(node->Polygon());
                close_path(polygon.contour);

                for (const auto& child : *node) {
                    if (child->IsHole())
                        process_hole(child.get(), polygon);
                    else
                        process_poly(child.get());
                }
                shapes.emplace_back(std::move(polygon));
            };

            for (const auto& child : tree) {
                process_poly(child.get());
            }
            return shapes;
        }

        double path_area(const Path& path) {
            if (path.size() < 3)
                return 0.0;

            double a = 0.0;
            auto i = path.begin();
            auto j = std::prev(path.end());
            while (i != path.end()) {
                const double xj = static_cast<double>(j->X);
                const double yj = static_cast<double>(j->Y);
                const double xi = static_cast<double>(i->X);
                const double yi = static_cast<double>(i->Y);
                a += (xj + xi) * (yj - yi);
                j = i;
                ++i;
            }
            a /= 2.0;
            return a;
        }
    }

    Path offset_path(const Path& path, Coord distance) {
        const c2::Paths64 result = c2::InflatePaths(c2::Paths64 {to_clipper_path(path)},
                                                    static_cast<double>(distance),
                                                    c2::JoinType::Square,
                                                    c2::EndType::Polygon);

        Polygon rebuilt;
        bool found_contour = false;
        for (const c2::Path64& candidate : result) {
            Path converted = from_clipper_path(candidate);
            if (!c2::IsPositive(candidate)) {
                std::reverse(converted.begin(), converted.end());
                close_path(converted);
                rebuilt.holes.emplace_back(std::move(converted));
                continue;
            }

            if (found_contour)
                continue;

            std::reverse(converted.begin(), converted.end());
            close_path(converted);
            rebuilt.contour = std::move(converted);
            found_contour = true;
        }

        return rebuilt.contour;
    }

    Shapes merge_shapes(const Shapes& shapes) {
        c2::Paths64 subjects;
        size_t count = 0;
        for (const Polygon& shape : shapes) {
            subjects.emplace_back(to_clipper_path(shape.contour));
            ++count;
            for (const Path& hole : shape.holes) {
                subjects.emplace_back(to_clipper_path(hole));
                ++count;
            }
        }

        c2::Clipper64 clipper;
        clipper.PreserveCollinear(false);
        clipper.ReverseSolution(true);
        clipper.AddSubject(subjects);

        c2::Paths64 open_paths;
        c2::PolyTree64 tree;
        if (!clipper.Execute(c2::ClipType::Union, c2::FillRule::Negative, tree, open_paths)) {
            return {};
        }

        return polytree_to_shapes(tree);
    }

    Shapes merge_shapes(const Shapes& shapes, const Polygon& polygon) {
        Shapes input = shapes;
        input.emplace_back(polygon);
        return merge_shapes(input);
    }

    Box bounding_box(const Path& path) {
        if (path.empty())
            return {};

        Point min = path.front();
        Point max = path.front();
        for (const Point& point : path) {
            min.X = std::min(min.X, point.X);
            min.Y = std::min(min.Y, point.Y);
            max.X = std::max(max.X, point.X);
            max.Y = std::max(max.Y, point.Y);
        }
        return {min, max};
    }

    Box bounding_box(const Polygon& polygon) {
        return bounding_box(polygon.contour);
    }

    Box bounding_box(const Shapes& shapes) {
        if (shapes.empty())
            return {};

        Box result;
        bool first = true;
        for (const Polygon& shape : shapes) {
            for (const Path* path_group : {&shape.contour}) {
                for (const Point& point : *path_group) {
                    if (first) {
                        result.min = point;
                        result.max = point;
                        first = false;
                    } else {
                        result.min.X = std::min(result.min.X, point.X);
                        result.min.Y = std::min(result.min.Y, point.Y);
                        result.max.X = std::max(result.max.X, point.X);
                        result.max.Y = std::max(result.max.Y, point.Y);
                    }
                }
            }
        }
        return result;
    }

    Box unite(const Box& a, const Box& b) {
        return {{std::min(a.min.X, b.min.X), std::min(a.min.Y, b.min.Y)},
                {std::max(a.max.X, b.max.X), std::max(a.max.Y, b.max.Y)}};
    }

    bool contains(const Box& host, const Box& guest) {
        return guest.min.X >= host.min.X && guest.max.X <= host.max.X && guest.min.Y >= host.min.Y &&
               guest.max.Y <= host.max.Y;
    }

    Path rotate_path(const Path& path, double radians) {
        const double cosa = std::cos(radians);
        const double sina = std::sin(radians);

        Path result;
        result.reserve(path.size());
        for (const Point& point : path) {
            result.emplace_back(static_cast<Coord>(point.X * cosa - point.Y * sina),
                                static_cast<Coord>(point.X * sina + point.Y * cosa));
        }
        return result;
    }

    Polygon rotate_polygon(const Polygon& polygon, double radians) {
        Polygon result;
        result.contour = rotate_path(polygon.contour, radians);
        for (const Path& hole : polygon.holes) {
            result.holes.emplace_back(rotate_path(hole, radians));
        }
        return result;
    }

    void translate_path(Path& path, const Point& delta) {
        for (Point& point : path)
            point += delta;
    }

    void translate_polygon(Polygon& polygon, const Point& delta) {
        translate_path(polygon.contour, delta);
        for (Path& hole : polygon.holes)
            translate_path(hole, delta);
    }

    Path translated_path(const Path& path, const Point& delta) {
        Path result = path;
        translate_path(result, delta);
        return result;
    }

    Point rightmost_top(const Path& path) {
        if (path.empty())
            return {0, 0};
        return *std::max_element(path.begin(), path.end(), vertex_less);
    }

    Point leftmost_bottom(const Path& path) {
        if (path.empty())
            return {0, 0};
        return *std::min_element(path.begin(), path.end(), vertex_less);
    }

    double polygon_area(const Polygon& polygon) {
        double result = path_area(polygon.contour);
        for (const Path& hole : polygon.holes)
            result += path_area(hole);
        return result;
    }

    PackingItem::PackingItem(const Path& contour) {
        m_shape.contour = contour;
    }

    const Polygon& PackingItem::inflated_shape() const {
        if (!m_has_inflation)
            return m_shape;
        if (m_inflate_cache_valid)
            return m_inflate_cache;

        m_inflate_cache = m_shape;
        m_inflate_cache.contour = offset_path(m_shape.contour, m_inflation);
        m_inflate_cache.holes.clear();
        m_inflate_cache_valid = true;
        return m_inflate_cache;
    }

    const Polygon& PackingItem::transformed_shape() const {
        if (m_transform_cache_valid)
            return m_transform_cache;

        Polygon result = inflated_shape();
        if (m_has_rotation)
            result = rotate_polygon(result, m_rotation);
        if (m_has_translation)
            translate_polygon(result, m_translation);

        m_transform_cache = std::move(result);
        m_transform_cache_valid = true;
        return m_transform_cache;
    }

    void PackingItem::inflate(Coord distance) {
        m_inflation += distance;
        m_has_inflation = true;
        invalidate();
    }

    double PackingItem::area() const {
        return polygon_area(inflated_shape());
    }

    Box PackingItem::bounding_box() const {
        if (!m_bbox_cache_valid) {
            m_bbox_cache = m_has_rotation ? packing::bounding_box(rotate_polygon(inflated_shape(), m_rotation))
                                          : packing::bounding_box(inflated_shape());
            m_bbox_cache_valid = true;
        }

        return {m_bbox_cache.min + m_translation, m_bbox_cache.max + m_translation};
    }

    Point PackingItem::reference_vertex() const {
        return rightmost_top(transformed_shape().contour);
    }

    Point PackingItem::rightmost_top_vertex() const {
        return reference_vertex();
    }

    Point PackingItem::leftmost_bottom_vertex() const {
        return leftmost_bottom(transformed_shape().contour);
    }

    void PackingItem::rotation(double value) {
        if (m_rotation == value)
            return;

        m_rotation = value;
        m_has_rotation = true;
        m_transform_cache_valid = false;
        m_bbox_cache_valid = false;
    }

    void PackingItem::translation(const Point& value) {
        if (m_translation == value)
            return;

        m_translation = value;
        m_has_translation = true;
        m_transform_cache_valid = false;
    }

    void PackingItem::invalidate() const {
        m_inflate_cache_valid = false;
        m_transform_cache_valid = false;
        m_bbox_cache_valid = false;
    }
}
