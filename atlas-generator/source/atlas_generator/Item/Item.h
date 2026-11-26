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

#include <cmath>
#include <filesystem>
#include <numbers>
#include <optional>
#include <stdint.h>
#include <vector>

namespace wk::AtlasGenerator {
    template <typename T>
    using Container = std::vector<T>;

    // Atlas sprite prepared for packing.
    //
    // Polygon invariant: after generate_image_polygon(), `vertices` is ALWAYS a
    // convex polygon with 4..8 vertices. Fallback is a 4-point rectangle. The
    // alpha path is a rectangle minus up to four corner cuts. 
    class Item {
    public:
        // Placement transform returned by the packer. Applies to `uv`, not `xy`.
        class Transformation {
        public:
            Transformation(double rotation = 0.0, Point translation = Point(0, 0));

        public:
            // Rotation in radians
            double rotation;
            Point translation;

            // Quarter turns use exact sign permutations. Trig near 90/180/270
            // degrees can push integer UV components past ceil().
            template <typename T>
            void transform_point(Point_t<T>& vertex) const {
                const double x = static_cast<double>(vertex.x);
                const double y = static_cast<double>(vertex.y);
                constexpr double half_pi = std::numbers::pi_v<double> / 2.0;
                constexpr double angle_epsilon = 1e-12;
                const double quarter = std::round(rotation / half_pi);

                double transformed_x;
                double transformed_y;

                if (std::abs(rotation - quarter * half_pi) <= angle_epsilon) {
                    const int steps = ((static_cast<int>(quarter) % 4) + 4) % 4;

                    switch (steps) {
                        case 0:
                            transformed_x = x + translation.x;
                            transformed_y = y + translation.y;
                            break;
                        case 1:
                            transformed_x = translation.x - y;
                            transformed_y = x + translation.y;
                            break;
                        case 2:
                            transformed_x = translation.x - x;
                            transformed_y = translation.y - y;
                            break;
                        default:
                            transformed_x = y + translation.x;
                            transformed_y = translation.y - x;
                            break;
                    }
                } else {
                    const double c = std::cos(rotation);
                    const double s = std::sin(rotation);

                    transformed_x = x * c - y * s + translation.x;
                    transformed_y = y * c + x * s + translation.y;
                }

                vertex.x = (T) std::ceil(transformed_x);
                vertex.y = (T) std::ceil(transformed_y);
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
        // Build from decoded image. `sliced` forces rectangle fallback and skips scaling.
        Item(const RawImage& image, bool sliced = false);

        // Create a solid-fill item. Uses a rectangular polygon.
        Item(const ColorRGBA& color);

        // Load from PNG path. Available only with image codecs enabled.
        Item(std::filesystem::path path, bool sliced = false);

        ~Item() = default;

    public:
        // Current preprocessing status. Valid means `vertices` is usable.
        Status status() const;

        // Current stored image size after scaling and cropping.
        uint16_t width() const;
        uint16_t height() const;

        // Backing image. Mutated only inside generate_image_polygon().
        const RawImage& image() const { return *m_image; }
        const RawImageRef& image_ref() const { return m_image; }

    public:
        // Atlas bin index. 0xFF means the item has not been packed.
        size_t texture_index = 0xFF;

        // Convex 4..8 point polygon. `uv` is packer/atlas space. `xy` is source space.
        Container<Vertex> vertices;

        // Rotation + translation returned by the packer.
        Transformation transform;

    public:
        // True for sliced items and tiny sprites (width + height < 100).
        bool is_rectangle() const;

        // True when the item came from a valid 9-slice guide.
        bool is_sliced() const;

        bool is_colorfill() const { return m_colorfill; }

        // Colorfill vertex, when generated.
        std::optional<AtlasGenerator::Vertex> get_colorfill() const;

    public:
        // Axis-aligned bounds over `vertices.xy`.
        Rect bound() const;

        // Axis-aligned bounds over `vertices.uv`.
        RectUV bound_uv() const;

        // Build the convex polygon from alpha.
        //
        // Steps: one-time scale/premultiply, threshold + crop alpha mask,
        // contour/hull, up to four corner half-plane cuts. Uses PointF and a
        // tiny convex clip, not general-purpose polygon boolean operations.
        // Always ends with a convex 4..8 vertex polygon and Status::Valid.
        void generate_image_polygon(const Config& config);

        // Accept externally supplied `vertices` and mark preprocessing done.
        bool mark_as_custom();

        bool mark_as_preprocessed();

    public:
        // Split a rectangular item into nine guide-clipped vertex paths.
        void get_9slice(const Rect& guide,
                        Container<Container<Vertex>>& vertices,
                        const Transformation xy_transform = Transformation()) const;

    public:
        // Equal when sharing the same image object or cached image hash.
        bool operator==(const Item& other) const;

    private:
        // Scale non-sliced images once and premultiply color channels.
        void image_preprocess(const Config& config);

        void alpha_preprocess();

        // Extract + threshold alpha in one pass and compute the nonzero bound.
        void extract_normalized_alpha(const Config& config,
                                      RawImageRef& mask,
                                      Image::Bound& crop_bound,
                                      bool& all_opaque,
                                      bool& any_opaque) const;

        // Collect boundary pixels of a binary alpha mask.
        void get_image_contour(RawImageRef image, Container<Point>& result);

        // Threshold alpha to 0/255 using Config::alpha_threshold().
        void normalize_mask(RawImageRef mask, const Config& config);

        // 5x5 dilation used before contour extraction.
        RawImageRef dilate_mask(RawImageRef mask);

        // Verify convexity and set Status::InvalidPolygon when it fails.
        bool verify_vertices();

        // Cached image hash used by duplicate detection.
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
