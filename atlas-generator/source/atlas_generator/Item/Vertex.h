#pragma once

#include "core/math/point.h"
#include "core/math/rect.h"

#include <stdint.h>

namespace wk::AtlasGenerator {
    using Rect = Rect_t<int32_t>;
    using RectF = Rect_t<float>;
    using Point = Point_t<int32_t>;

    using RectUV = Rect_t<uint16_t>;
    using PointUV = Point_t<uint16_t>;

    template <typename T>
    class Vertex_t {
    public:
        Vertex_t() {
            xy = Point_t<T>(0, 0);
            uv = PointUV(0, 0);
        };

        Vertex_t(T x, T y, uint16_t u, uint16_t v) {
            xy = Point_t<T>(x, y);
            uv = PointUV(u, v);
        };

    public:
        PointUV uv;
        Point_t<T> xy;
    };

    using Vertex = Vertex_t<int32_t>;
    using VertexF = Vertex_t<float>;
}
