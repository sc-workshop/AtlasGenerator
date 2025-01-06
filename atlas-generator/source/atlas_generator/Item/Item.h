#pragma once

#include <stdint.h>
#include <vector>
#include <filesystem>
#include <optional>
#include "core/image/raw_image.h"
#include "core/math/color_rgba.h"
#include "core/memory/ref.h"

#include "Vertex.h"
#include "atlas_generator/Config.h"
#include "core/math/point.h"
#include "core/math/rect.h"

namespace wk
{
	namespace AtlasGenerator
	{
		template<typename T>
		using Container = std::vector<T>;

		// Interface that represents Atlas Sprite
		class Item
		{
		public:
			class Transformation
			{
			public:
				Transformation(double rotation = 0.0, Point translation = Point(0, 0));

			public:
				// Rotation in radians
				double rotation;
				Point translation;

				template<typename T>
				void transform_point(Point_t<T>& vertex) const
				{
					T x = vertex.x;
					T y = vertex.y;

					vertex.x = (T)std::ceil(x * std::cos(rotation) - y * std::sin(rotation) + translation.x);
					vertex.y = (T)std::ceil(y * std::cos(rotation) + x * std::sin(rotation) + translation.y);
				}
			};
		public:
			enum class Status : uint8_t
			{
				Unset = 0,
				Valid,
				InvalidPolygon
			};

			enum FixedRotation : uint16_t
			{
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
			Transformation transform;

		public:
			bool is_rectangle() const;
			bool is_sliced() const;
			bool is_colorfill() const { return m_colorfill; };
			std::optional< AtlasGenerator::Vertex> get_colorfill() const;

		public:
			// XY coords bound
			Rect bound() const;
			RectUV bound_uv() const;
			void generate_image_polygon(const Config& config);
			bool mark_as_custom();
			bool mark_as_preprocessed();

		public:
			void get_9slice(
				const Rect& guide,
				Container<Container<Vertex>>& vertices,
				const Transformation xy_transform = Transformation()
			) const;

		public:
			bool operator ==(const Item& other) const;

		private:
			void image_preprocess(const Config& config);
			void alpha_preprocess();

			void get_image_contour(RawImageRef& image, Container<Point>& result);

			void normalize_mask(RawImageRef& mask, const Config& config);
			void dilate_mask(RawImageRef& mask);

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
}