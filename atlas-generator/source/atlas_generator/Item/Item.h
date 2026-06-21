#pragma once

#include "Vertex.h"
#include "atlas_generator/Config.h"
#include "core/geometry/convex.hpp"
#include "core/geometry/intersect.hpp"
#include "core/image/raw_image.h"
#include "core/math/color_rgba.h"
#include "core/math/point.h"
#include "core/math/rect.h"
#include "core/memory/ref.h"

#include <clipper2/clipper.h>
#include <filesystem>
#include <optional>
#include <stdint.h>
#include <vector>

namespace wk::AtlasGenerator {
    template <typename T>
    using Container = std::vector<T>;

    // Interface that represents Atlas Sprite
    class Item {
    public:
        template <typename T = int32_t>
        class Transformation {
        public:
            Transformation(double rotation = 0.0, Point_t<T> translation = Point_t<T>(0, 0)) :
                rotation(rotation),
                translation(translation) {}

        public:
            // Rotation in radians
            double rotation;
            Point_t<T> translation;

            template <typename P>
            void transform_point(Point_t<P>& vertex) const {
                P x = vertex.x;
                P y = vertex.y;

                vertex.x = (T) std::ceil(x * std::cos(rotation) - y * std::sin(rotation) + translation.x);
                vertex.y = (T) std::ceil(y * std::cos(rotation) + x * std::sin(rotation) + translation.y);
            }
        };

    public:
        enum class Status : uint8_t {
            Unset = 0,
            Valid,
            InvalidPolygon
        };

        enum FixedRotation : uint16_t {
            NoRotation = 0,
            Rotation90 = 90,
            Rotation180 = 180,
            Rotation270 = 270
        };

    public:
        Item(const RawImage& image, bool sliced = false);
        Item(const ColorRGBA& color);
        Item(std::filesystem::path path, bool sliced = false);

        ~Item() = default;

        // Image Info
    public:
        Status status() const;
        uint16_t width() const;
        uint16_t height() const;

        const RawImage& image() const { return *m_image; };
        const RawImageRef& image_ref() const { return m_image; };

        // Generator Info
    public:
        size_t texture_index = 0xFF;
        Container<Vertex> vertices;

        // UV Transformation
        Transformation<int32_t> transform;

    public:
        bool is_rectangle() const;
        bool is_sliced() const;
        bool is_colorfill() const { return m_colorfill; };
        std::optional<AtlasGenerator::Vertex> get_colorfill() const;

    public:
        // XY coords bound
        RectF bound() const;
        RectUV bound_uv() const;
        void generate_image_polygon(const Config& config);
        bool mark_as_custom();
        bool mark_as_preprocessed();

    public:
        /// @brief Splits provided vertex array into 9 slices accroding to provided guide
        /// @param guide Slice guide
        /// @param regions Output splited regions
        /// @param vertices Polygon vertoces
        /// @param xy_transform Vertices transformation
        template <typename R, typename T>
        static void Generate9Slice(const RectF& guide,
                                   Container<Container<Vertex_t<R>>>& result,
                                   const Container<Vertex_t<T>>& vertices,
                                   const Transformation<float> xy_transform = Transformation<float>()) {
            using namespace Clipper2Lib;

            PathsD result_solution;
            {
                PathD subject;

                subject.reserve(vertices.size());
                for (const auto& vertex : vertices) {
                    subject.emplace_back(vertex.xy.x + xy_transform.translation.x,
                                         vertex.xy.y + xy_transform.translation.y);
                }

                constexpr float min = (float) std::numeric_limits<int>::min();
                constexpr float max = (float) std::numeric_limits<int>::max();

                const Container<RectF> rects = {
                    {min, min, guide.left, guide.bottom},       // Left-Top
                    {min, guide.bottom, guide.left, guide.top}, // Top-Middle
                    {guide.left, guide.top, min, max},          // Right-Top

                    {guide.left, min, guide.right, guide.bottom},       // Left-Middle
                    {guide.left, guide.bottom, guide.right, guide.top}, // Middle
                    {guide.left, guide.top, guide.right, max},          // Middle-bottom

                    {guide.right, guide.bottom, max, min},       // Left-bottom
                    {guide.right, guide.top, max, guide.bottom}, // Middle-bottom
                    {guide.right, guide.top, max, max},          // Right-bottom

                };

                for (const RectF& rect : rects) {
                    PathD path;

                    path.emplace_back(rect.bottom, rect.left);
                    path.emplace_back(rect.bottom, rect.right);
                    path.emplace_back(rect.top, rect.right);
                    path.emplace_back(rect.top, rect.left);

                    PathsD solution = Intersect({subject}, {path}, FillRule::NonZero, 8);
                    result_solution.insert(result_solution.end(), solution.begin(), solution.end());
                }
            }

            for (PathD& path : result_solution) {
                Container<Vertex_t<R>>& result_path = result.emplace_back();
                for (const PointD& path_vertex : path) {
                    Vertex_t<R>& vertex = result_path.emplace_back();

                    vertex.xy.x = (R) path_vertex.x;
                    vertex.xy.y = (R) path_vertex.y;
                }
            }
        }

        /// @brief Splits current item polygon to 9 slices according to provided guide
        /// @param guide Slice guide
        /// @param regions Output splited regions
        /// @param xy_transform Vertices transformation
        void get_9slice(const RectF& guide,
                        Container<Container<VertexF>>& regions,
                        const Transformation<float> xy_transform = Transformation<float>()) const;

    public:
        bool operator==(const Item& other) const;

    private:
        void image_preprocess(const Config& config);
        void alpha_preprocess();

        void get_image_contour(RawImageRef image, Container<Point>& result);

        void normalize_mask(RawImageRef mask, const Config& config);
        RawImageRef dilate_mask(RawImageRef mask);

        bool verify_vertices();

        std::size_t hash() const;

    protected:
        Status m_status = Status::Unset;
        bool m_preprocessed = false;
        bool m_sliced = false;
        bool m_colorfill = false;

        RawImageRef m_image;
        mutable size_t m_hash = 0;
    };
}
