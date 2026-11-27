#pragma once

#include "core/preprocessor/api.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace wk::AtlasGenerator::packing {
    using Coord = int64_t;

    struct Point {
        Coord X = 0;
        Coord Y = 0;

        Point() = default;
        Point(Coord x, Coord y) :
            X(x),
            Y(y) {}

        bool operator==(const Point& other) const { return X == other.X && Y == other.Y; }
    };

    using Path = std::vector<Point>;
    using Paths = std::vector<Path>;

    struct Polygon {
        Path contour;
        Paths holes;
    };

    using Shapes = std::vector<Polygon>;

    constexpr int BIN_ID_UNSET = -1;

    WK_INLINE Point operator+(const Point& a, const Point& b) {
        return {a.X + b.X, a.Y + b.Y};
    }

    WK_INLINE Point operator-(const Point& a, const Point& b) {
        return {a.X - b.X, a.Y - b.Y};
    }

    WK_INLINE Point& operator+=(Point& a, const Point& b) {
        a.X += b.X;
        a.Y += b.Y;
        return a;
    }

    WK_INLINE Point& operator-=(Point& a, const Point& b) {
        a.X -= b.X;
        a.Y -= b.Y;
        return a;
    }

    WK_INLINE bool vertex_less(const Point& a, const Point& b) {
        return a.Y == b.Y ? a.X < b.X : a.Y < b.Y;
    }

    struct Box {
        Point min {0, 0};
        Point max {0, 0};

        Box() = default;
        Box(const Point& min_corner, const Point& max_corner) :
            min(min_corner),
            max(max_corner) {}

        Box(Coord width, Coord height, const Point& center) :
            min(center - Point {width / 2, height / 2}),
            max(center + Point {width / 2 + width % 2, height / 2 + height % 2}) {}

        Coord width() const { return max.X - min.X; }
        Coord height() const { return max.Y - min.Y; }

        double area() const { return static_cast<double>(width()) * static_cast<double>(height()); }

        Point center() const { return {(min.X + max.X) / 2, (min.Y + max.Y) / 2}; }
    };

    Path offset_path(const Path& path, Coord distance);
    Shapes merge_shapes(const Shapes& shapes);
    Shapes merge_shapes(const Shapes& shapes, const Polygon& polygon);

    Box bounding_box(const Path& path);
    Box bounding_box(const Polygon& polygon);
    Box bounding_box(const Shapes& shapes);
    Box unite(const Box& a, const Box& b);
    bool contains(const Box& host, const Box& guest);

    Path rotate_path(const Path& path, double radians);
    Polygon rotate_polygon(const Polygon& polygon, double radians);
    void translate_path(Path& path, const Point& delta);
    void translate_polygon(Polygon& polygon, const Point& delta);
    Path translated_path(const Path& path, const Point& delta);
    Point rightmost_top(const Path& path);
    Point leftmost_bottom(const Path& path);
    double polygon_area(const Polygon& polygon);

    // Local clone of the active packing item state machine
    class PackingItem {
    public:
        PackingItem();
        explicit PackingItem(const Path& contour);

        void prepare();

        const Path& shape() const { return m_shape.contour; }
        const Polygon& inflated_shape() const { return m_inflate_cache; }
        const Polygon& transformed_shape() const { return m_transform_cache; }

        void inflate(Coord distance);
        double area() const;
        Box bounding_box() const;
        Point reference_vertex() const;
        Point rightmost_top_vertex() const;
        Point leftmost_bottom_vertex() const;

        double rotation() const { return m_rotation; }
        void rotation(double value);
        Point translation() const { return m_translation; }
        void translation(const Point& value);
        void translate(const Point& delta) { translation(translation() + delta); }

        int bin_id() const { return m_bin_id; }
        void bin_id(int id) { m_bin_id = id; }

    private:
        void mark_dirty();

        Polygon m_shape;
        Coord m_inflation = 0;
        bool m_has_inflation = false;
        double m_rotation = 0.0;
        bool m_has_rotation = false;
        Point m_translation {0, 0};
        bool m_has_translation = false;
        int m_bin_id = BIN_ID_UNSET;

        Polygon m_inflate_cache;
        Polygon m_transform_cache;
        Box m_bbox_cache;
        bool m_prepared = false;
    };
}
