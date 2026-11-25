#include "atlas_generator/Item/Item.h"

#include "core/geometry/clip.hpp"
#include "core/io/file_stream.h"
#include "core/math/triangle.h"
#include "core/stb/stb.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace wk { namespace AtlasGenerator {
    Item::Transformation::Transformation(double rotation, Point translation) :
        rotation(rotation),
        translation(translation) {
    }

    Item::Item(const RawImage& image, bool sliced) :
        m_sliced(sliced),
        m_image(wk::CreateRef<RawImage>(image)) {
    }

    Item::Item(const ColorRGBA& color) :
        m_image(wk::CreateRef<RawImage>(color)),
        m_colorfill(true) {
    }

#ifdef ATLAS_GENERATOR_WITH_IMAGE_CODECS
    Item::Item(std::filesystem::path path, bool sliced) :
        m_sliced(sliced) {
        InputFileStream file(path);
        stb::load_image(file, m_image);
    }
#endif

    Item::Status Item::status() const {
        return m_status;
    }
    uint16_t Item::width() const {
        return (uint16_t) m_image->width();
    };
    uint16_t Item::height() const {
        return (uint16_t) m_image->height();
    };

    bool Item::is_rectangle() const {
        if (is_sliced())
            return true;

        return 100 > width() + height();
    };

    bool Item::is_sliced() const {
        return m_sliced;
    }

    std::optional<AtlasGenerator::Vertex> Item::get_colorfill() const {
        if (vertices.size() >= 1 && m_colorfill) {
            return vertices[vertices.size() - 1];
        }

        return std::nullopt;
    }

    bool Item::mark_as_custom() {
        // if (!verify_vertices()) return false;

        m_status = Status::Valid;
        mark_as_preprocessed();
        return true;
    }

    bool Item::mark_as_preprocessed() {
        m_preprocessed = true;
        return true;
    }

    void Item::generate_image_polygon(const Config& config) {
        using namespace wk::Geometry;

        float scale_factor = is_sliced() ? 1.0f : config.scale();
        Image::Size full_size = m_image->size();
        Image::Size current_size = full_size;
        PointF crop_offset(0.0f, 0.0f);

        auto fallback_rectangle = [this, &crop_offset, &full_size, &current_size] {
            vertices.resize(4);

            vertices[3].uv = {0, 0};
            vertices[2].uv = {0, (uint16_t) current_size.y};
            vertices[1].uv = {(uint16_t) current_size.x, (uint16_t) current_size.y};
            vertices[0].uv = {(uint16_t) current_size.x, 0};

            vertices[3].xy = {(uint16_t) crop_offset.x, (uint16_t) crop_offset.y};
            vertices[2].xy = {(uint16_t) crop_offset.x, (uint16_t) (crop_offset.y + current_size.y)};
            vertices[1].xy = {(uint16_t) (crop_offset.x + current_size.x), (uint16_t) (crop_offset.y + current_size.y)};
            vertices[0].xy = {(uint16_t) (crop_offset.x + current_size.x), (uint16_t) crop_offset.y};

            m_status = Status::Valid;
        };
        image_preprocess(config);

        if (1 >= width() || 1 >= height()) {
            fallback_rectangle();
            return;
        }

        RawImageRef alpha_mask;
        Image::Bound crop_bound;
        bool all_opaque = false;
        bool any_opaque = false;

        switch (m_image->channels()) {
            case 4:
            case 2:
                extract_normalized_alpha(config, alpha_mask, crop_bound, all_opaque, any_opaque);
                break;
            default:
                fallback_rectangle();
                return;
        }

        if (crop_bound.width <= 0)
            crop_bound.width = 1;
        if (crop_bound.height <= 0)
            crop_bound.height = 1;

        // Fully opaque sprites always end as the full rectangle.
        if (all_opaque) {
            current_size = alpha_mask->size();
            fallback_rectangle();
            return;
        }

        // Fully transparent sprites crop to the same 1x1 rectangle as before.
        if (!any_opaque) {
            m_image = m_image->crop(crop_bound);
            current_size = {1, 1};
            fallback_rectangle();
            return;
        }

        // Image cropping by alpha
        if (m_image->width() > crop_bound.width || m_image->height() > crop_bound.height) {
            m_image = m_image->crop(crop_bound);
            alpha_mask = alpha_mask->crop(crop_bound);
        }

        current_size = alpha_mask->size();
        crop_offset = PointF(crop_bound.x * scale_factor, crop_bound.y * scale_factor);

        if (is_rectangle()) {
            fallback_rectangle();
            return;
        } else {
            // Alpha path: rectangle minus up to four corner triangles.
            // Result remains convex, so NFP gets 4..8 vertices maximum.
            vertices.clear();
            vertices.reserve(8);
        }

        Container<Point> polygon;
        {
            Container<Point> contour;
            get_image_contour(dilate_mask(alpha_mask), contour);

            // Getting convex hull as base polygon for calculations
            polygon = Hull::quick_hull(contour);
        }

        Container<Triangle> triangles;
        triangles.reserve(4);

        Point centroid = {static_cast<int>(abs(static_cast<float>(current_size.x) / 2)),
                          static_cast<int>(abs(static_cast<float>(current_size.y) / 2))};

        float distance_threshold = (current_size.x + current_size.y) * 0.03f;

        auto calculate_triangle =
            [&centroid, &current_size, &polygon, &triangles, &distance_threshold, this](Point input_point) {
                LineF ray(PointF((float) input_point.x, (float) input_point.y),
                          PointF((float) centroid.x, (float) centroid.y));

                auto intersection_result = line_intersect(polygon, ray);
                if (!intersection_result.has_value())
                    return;

                auto& [p1_idx, p2_idx, intersect] = intersection_result.value();

                const Point& p1 = polygon[p1_idx];
                const Point& p2 = polygon[p2_idx];

                // Skip processing if distance between corner and intersect point is too smol
                {
                    float distance = dist(input_point, intersect);

                    if (distance_threshold > distance) {
                        return;
                    }
                }

                float angle = line_angle(Line(p1, p2));

                Line cutoff_bisector = Line(input_point, Point((int) ceil(intersect.x), (int) ceil(intersect.y)));

                Triangle cutoff = build_triangle(cutoff_bisector, angle, (current_size.x + current_size.y) * 2);

                triangles.push_back(cutoff);
            };

        calculate_triangle(Point(0, 0));
        calculate_triangle(Point(current_size.x, 0));
        calculate_triangle(Point(current_size.x, current_size.y));
        calculate_triangle(Point(0, current_size.y));

        if (triangles.empty()) {
            fallback_rectangle();
            return;
        }

        // Rectangle minus up to four corner half-planes. The result stays
        // convex and has at most eight vertices.
        {
            std::vector<PointF> clipped;
            clipped.reserve(8);
            clipped.emplace_back(0.0f, 0.0f);
            clipped.emplace_back((float) current_size.x, 0.0f);
            clipped.emplace_back((float) current_size.x, (float) current_size.y);
            clipped.emplace_back(0.0f, (float) current_size.y);

            for (const Triangle& triangle : triangles) {
                clipped = Geometry::clip_polygon_by_line(clipped,
                                                         PointF((float) triangle.p2.x, (float) triangle.p2.y),
                                                         PointF((float) triangle.p3.x, (float) triangle.p3.y),
                                                         PointF((float) triangle.p1.x, (float) triangle.p1.y));

                if (clipped.empty())
                    break;
            }

            int cut_mask = 0;
            for (const Triangle& triangle : triangles) {
                if (triangle.p1 == Point(0, 0))
                    cut_mask |= 1;
                else if (triangle.p1 == Point(current_size.x, 0))
                    cut_mask |= 2;
                else if (triangle.p1 == Point(current_size.x, current_size.y))
                    cut_mask |= 4;
                else if (triangle.p1 == Point(0, current_size.y))
                    cut_mask |= 8;
            }

            // Ring start is part of the emitted polygon format; the corner-cut
            // mask determines which clipped vertex must come first.
            size_t rotate_by = 0;
            if (cut_mask == 2 || cut_mask == 4)
                rotate_by = 1;
            else if (cut_mask == 15 && 1 < clipped.size())
                rotate_by = clipped.size() - 1;

            if (!clipped.empty() && rotate_by) {
                std::rotate(clipped.begin(), clipped.begin() + rotate_by, clipped.end());
            }

            vertices.reserve(clipped.size());
            for (const PointF& point : clipped) {
                // Snap near-integer intersections before `ceil()` so float drift cannot move a texel boundary.
                auto snap_integer = [](double v) {
                    const double rounded = std::round(v);
                    return std::abs(v - rounded) < 0.001 ? rounded : v;
                };
                const double qx = snap_integer(point.x);
                const double qy = snap_integer(point.y);

                int32_t x = (int32_t) std::ceil((qx + crop_bound.x) * scale_factor);
                int32_t y = (int32_t) std::ceil((qy + crop_bound.y) * scale_factor);
                uint16_t u = (uint16_t) std::ceil(qx);
                uint16_t v = (uint16_t) std::ceil(qy);

                vertices.emplace_back(x, y, u, v);
            }
        }

        if (vertices.empty()) {
            fallback_rectangle();
        } else {
            m_status = Status::Valid;
        }
    };

    Rect Item::bound() const {
        Rect result(std::numeric_limits<int>::max(), 0, 0, std::numeric_limits<int>::max());

        for (const Vertex& vertex : vertices) {
            result.left = std::min(result.left, vertex.xy.x);
            result.bottom = std::min(result.bottom, vertex.xy.y);
            result.right = std::max(result.right, vertex.xy.x);
            result.top = std::max(result.top, vertex.xy.y);
        }

        return result;
    }

    RectUV Item::bound_uv() const {
        RectUV result(std::numeric_limits<uint16_t>::max(), 0, 0, std::numeric_limits<uint16_t>::max());

        for (const Vertex& vertex : vertices) {
            result.left = std::min(result.left, vertex.uv.x);
            result.bottom = std::min(result.bottom, vertex.uv.y);
            result.right = std::max(result.right, vertex.uv.x);
            result.top = std::max(result.top, vertex.uv.y);
        }

        return result;
    }

    void Item::get_9slice(const Rect& guide,
                          Container<Container<Vertex>>& result,
                          const Transformation xy_transform) const {
        if (!is_rectangle())
            return;

        Point offset;
        Point size;
        {
            Rect xy_bound = bound();
            xy_bound.left += xy_transform.translation.x;
            xy_bound.right += xy_transform.translation.x;
            xy_bound.bottom += xy_transform.translation.y;
            xy_bound.top += xy_transform.translation.y;

            offset.x = xy_bound.left;
            offset.y = xy_bound.bottom;
            size.x = std::abs(xy_bound.left - xy_bound.right);
            size.y = std::abs(xy_bound.top - xy_bound.bottom);
        }

        PointUV uv_size;
        {
            RectUV uv_bound = bound_uv();

            uv_size.x = (uint16_t) std::abs(uv_bound.left - uv_bound.right);
            uv_size.y = (uint16_t) std::abs(uv_bound.top - uv_bound.bottom);
        }

        Container<PointF> subject;
        subject.reserve(vertices.size());
        for (const Vertex& vertex : vertices) {
            subject.emplace_back((float) (vertex.xy.x + xy_transform.translation.x),
                                 (float) (vertex.xy.y + xy_transform.translation.y));
        }

        constexpr int min = std::numeric_limits<int>::min();
        constexpr int max = std::numeric_limits<int>::max();

        const Container<Rect> rects = {
            {min, min, guide.left, guide.bottom},
            {min, guide.bottom, guide.left, guide.top},
            {guide.left, guide.top, min, max},

            {guide.left, min, guide.right, guide.bottom},
            {guide.left, guide.bottom, guide.right, guide.top},
            {guide.left, guide.top, guide.right, max},

            {guide.right, guide.bottom, max, min},
            {guide.right, guide.top, max, guide.bottom},
            {guide.right, guide.top, max, max},
        };

        for (size_t rect_index = 0; rect_index < rects.size(); rect_index++) {
            const Rect& rect = rects[rect_index];
            const float left = (float) std::min(rect.bottom, rect.top);
            const float right = (float) std::max(rect.bottom, rect.top);
            const float bottom = (float) std::min(rect.left, rect.right);
            const float top = (float) std::max(rect.left, rect.right);

            Container<PointF> clipped = subject;
            clipped = Geometry::clip_polygon_by_line(clipped,
                                                     PointF(left, bottom),
                                                     PointF(left, top),
                                                     PointF(left - 1.0f, bottom));
            if (clipped.empty())
                continue;
            clipped = Geometry::clip_polygon_by_line(clipped,
                                                     PointF(right, bottom),
                                                     PointF(right, top),
                                                     PointF(right + 1.0f, bottom));
            if (clipped.empty())
                continue;
            clipped = Geometry::clip_polygon_by_line(clipped,
                                                     PointF(left, bottom),
                                                     PointF(right, bottom),
                                                     PointF(left, bottom - 1.0f));
            if (clipped.empty())
                continue;
            clipped = Geometry::clip_polygon_by_line(clipped,
                                                     PointF(left, top),
                                                     PointF(right, top),
                                                     PointF(left, top + 1.0f));
            if (clipped.empty())
                continue;

            // Guide paths use a stable ring start: the rightmost vertex first.
            // Y tie-break follows the cell row: upper for most cells, lower
            // for the two bottom-row cells.
            const bool start_at_bottom = 6 == rect_index || 7 == rect_index;
            size_t start = 0;
            for (size_t i = 1; clipped.size() > i; i++) {
                const bool better_x = clipped[i].x > clipped[start].x;
                const bool same_x = clipped[i].x == clipped[start].x;
                const bool better_y =
                    start_at_bottom ? clipped[i].y < clipped[start].y : clipped[i].y > clipped[start].y;

                if (better_x || (same_x && better_y)) {
                    start = i;
                }
            }
            std::rotate(clipped.begin(), clipped.begin() + start, clipped.end());

            Container<Vertex>& result_path = result.emplace_back();
            for (const PointF& path_vertex : clipped) {
                Vertex& vertex = result_path.emplace_back();

                vertex.xy.x = (int) path_vertex.x;
                vertex.xy.y = (int) path_vertex.y;

                // Remap xy to uv map, normalized to 0.0...1.0 range.
                const PointF uv_coord((path_vertex.x - offset.x) / size.x, (path_vertex.y - offset.y) / size.y);

                vertex.uv.x = (uint16_t) (uv_coord.x * uv_size.x);
                vertex.uv.y = (uint16_t) (uv_coord.y * uv_size.y);
            }
        }
    }

    bool Item::operator==(const Item& other) const {
        if (std::addressof(image()) == std::addressof(other.image()))
            return true;

        if (hash() != other.hash())
            return false;

        return true;
    }

    void Item::image_preprocess(const Config& config) {
        if (m_preprocessed)
            return;
        if (config.scale() != 1.0f && !is_sliced()) {
            RawImageRef resized = CreateRef<RawImage>((uint16_t) ceil(m_image->width() * config.scale()),
                                                      (uint16_t) ceil(m_image->height() * config.scale()),
                                                      m_image->depth(),
                                                      m_image->colorspace());

            m_image->copy(*resized);
            m_image = resized;
        }

        int channels = m_image->channels();

        if (channels == 2 || channels == 4) {
            alpha_preprocess();
        }

        m_preprocessed = true;
    }
    void Item::alpha_preprocess() {
        int channels = m_image->channels();

        for (uint16_t h = 0; height() > h; h++) {
            for (uint16_t w = 0; width() > w; w++) {
                switch (channels) {
                    case 4: {
                        ColorRGBA& pixel = m_image->at<ColorRGBA>(w, h);
                        float alpha = (float) pixel.a / 255.f;

                        pixel.r = (uint8_t) (pixel.r * alpha);
                        pixel.g = (uint8_t) (pixel.g * alpha);
                        pixel.b = (uint8_t) (pixel.b * alpha);

                    } break;
                    case 2: {
                        ColorLA& pixel = m_image->at<ColorLA>(w, h);
                        float alpha = (float) pixel.a / 255.f;

                        pixel.l = (uint8_t) (pixel.l * alpha);
                    } break;
                }
            }
        }
    }

    void Item::extract_normalized_alpha(
        const Config& config, RawImageRef& mask, Image::Bound& crop_bound, bool& all_opaque, bool& any_opaque) const {
        const uint8_t alpha_channel = (4 == m_image->channels()) ? 3 : 1;
        const Image::SizeT width = m_image->width();
        const Image::SizeT height = m_image->height();
        const uint8_t threshold = config.alpha_threshold();

        mask = wk::CreateRef<RawImage>(width, height, Image::PixelDepth::LUMINANCE8, m_image->colorspace());

        Image::Bound nonzero_bound(width, 0, 0, height);
        all_opaque = true;
        any_opaque = false;

        for (Image::SizeT h = 0; height > h; h++) {
            for (Image::SizeT w = 0; width > w; w++) {
                const uint8_t alpha = m_image->at(w, h)[alpha_channel];
                const bool opaque = alpha > threshold;

                mask->at<uint8_t>(w, h) = opaque ? 255 : 0;

                if (!opaque) {
                    all_opaque = false;
                    continue;
                }

                any_opaque = true;
                nonzero_bound.left = std::min(nonzero_bound.left, w);
                nonzero_bound.right = std::max(nonzero_bound.right, w);
                // Rect_t unions `bottom` with height and `top` with y.
                // RawImage::bound() stores min-y in `bottom` and max-y in `top`.
                nonzero_bound.bottom = std::min(nonzero_bound.bottom, h);
                nonzero_bound.top = std::max(nonzero_bound.top, h);
            }
        }

        crop_bound = any_opaque
                         ? Image::Bound(nonzero_bound.left,
                                        nonzero_bound.bottom,
                                        (uint16_t) std::abs((int) nonzero_bound.left - (int) nonzero_bound.right) + 1,
                                        (uint16_t) std::abs((int) nonzero_bound.top - (int) nonzero_bound.bottom) + 1)
                         : Image::Bound();
    }

    void Item::get_image_contour(RawImageRef image, Container<Point>& result) {
        result.reserve(result.size() + 2 * ((size_t) image->width() + (size_t) image->height()));
        for (Image::SizeT h = 0; image->height() > h; h++) {
            for (Image::SizeT w = 0; image->width() > w; w++) {
                uint8_t pixel = image->at<uint8_t>(w, h);
                bool valid = false;

                // Iterate over black pixels only
                if (pixel > 1) {
                    bool edge_width = w >= image->width() - 1;
                    bool edge_height = h >= image->height() - 1;
                    if (h == 0 || w == 0 || edge_width || edge_height) {
                        result.emplace_back(edge_width ? w + 1 : w, edge_height ? h + 1 : h);
                        continue;
                    }
                } else {
                    continue;
                };

                bool has_positive = false;
                bool has_negative = false;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int x = w + dy;
                        int y = h + dx;

                        if (dx == 0 && dy == 0)
                            continue;
                        if (0 > x || 0 > y)
                            continue;
                        if (x >= image->width() || y >= image->height())
                            continue;

                        uint8_t neighbor = image->at<uint8_t>((Image::SizeT) x, (Image::SizeT) y);

                        has_positive = has_positive || neighbor > 0;
                        has_negative = has_negative || neighbor == 0;
                    }
                }

                valid = has_negative && has_positive;
                if (valid) {
                    result.emplace_back(w, h);
                }
            }
        }
    }

    void Item::normalize_mask(RawImageRef mask, const Config& config) {
        // Pixel Normalize
        for (uint16_t h = 0; mask->height() > h; h++) {
            for (uint16_t w = 0; mask->width() > w; w++) {
                uint8_t& pixel = mask->at<uint8_t>(w, h);

                if (pixel > config.alpha_threshold()) {
                    pixel = 255;
                } else {
                    pixel = 0;
                };
            }
        }
    }

    RawImageRef Item::dilate_mask(RawImageRef mask) {
        RawImageRef result = wk::CreateRef<RawImage>(mask->width(), mask->height(), mask->depth());

        // Same 5x5 neighborhood as before, without kernel lookups or center skip checks.
        static const std::array<Point, 24> offsets = {Point {-2, -2}, Point {-1, -2}, Point {0, -2},  Point {1, -2},
                                                      Point {2, -2},  Point {-2, -1}, Point {-1, -1}, Point {0, -1},
                                                      Point {1, -1},  Point {2, -1},  Point {-2, 0},  Point {-1, 0},
                                                      Point {1, 0},   Point {2, 0},   Point {-2, 1},  Point {-1, 1},
                                                      Point {0, 1},   Point {1, 1},   Point {2, 1},   Point {-2, 2},
                                                      Point {-1, 2},  Point {0, 2},   Point {1, 2},   Point {2, 2}};

        const int32_t mask_width = (int32_t) mask->width();
        const int32_t mask_height = (int32_t) mask->height();

        for (uint16_t h = 0; h < mask->height(); h++) {
            for (uint16_t w = 0; w < mask->width(); w++) {
                if (mask->at<uint8_t>(w, h) != 0xFF)
                    continue;

                for (const Point& offset : offsets) {
                    const int32_t dstW = (int32_t) w + offset.x;
                    const int32_t dstH = (int32_t) h + offset.y;

                    if ((dstW >= 0 && dstW < mask_width) && (dstH >= 0 && dstH < mask_height)) {
                        result->at<uint8_t>((uint16_t) dstW, (uint16_t) dstH) = 0xFF;
                    }
                }
            }
        }

        return result;
    }

    bool Item::verify_vertices() {
        std::vector<PointUV> points;
        points.resize(vertices.size());
        for (size_t i = 0; vertices.size() > i; i++) {
            points[i] = vertices[i].uv;
        }

        if (Geometry::get_polygon_type(points) != Geometry::PolygonType::Convex) {
            m_status = Status::InvalidPolygon;
            return false;
        }

        return true;
    }

    std::size_t Item::hash() const {
        if (!m_hash) {
            m_hash = m_image->hash();
        }

        return m_hash;
    }
}}