#pragma once

#include <stdint.h>
#include <vector>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <optional>

#include "Vertex.h"
#include "atlas_generator/Config.h"
#include "core/math/point.h"
#include "core/math/rect.h"

#ifdef CV_DEBUG
void ShowContour(cv::Mat& src, std::vector<wk::AtlasGenerator::Vertex>& points);
void ShowContour(cv::Mat& src, std::vector<wk::Point>& points);
void ShowContour(cv::Mat& src, std::vector<cv::Point> points);
void ShowImage(std::string name, cv::Mat& image);
#endif

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

					vertex.x = (T)ceil(x * std::cos(rotation) - y * std::sin(rotation) + translation.x);
					vertex.y = (T)ceil(y * std::cos(rotation) + x * std::sin(rotation) + translation.y);
				}
			};
		public:
			enum class Status : uint8_t
			{
				Unset = 0,
				Valid,
				InvalidPolygon
			};

		public:
			Item(cv::Mat& image, bool sliced = false);
			Item(cv::Scalar color);
			Item(std::filesystem::path path, bool sliced = false);

			virtual ~Item() = default;

			// Image Info
		public:
			Status status() const;
			virtual uint16_t width() const;
			virtual uint16_t height() const;

			virtual cv::Mat& image();

			// Generator Info
		public:
			size_t texture_index = 0xFF;
			Container<Vertex> vertices;

			// UV Transformation
			Transformation transform;

		public:
			bool is_rectangle() const;
			bool is_sliced() const;
			std::optional< AtlasGenerator::Vertex> get_colorfill() const;

		public:
			// XY coords bound
			Rect bound() const;
			void generate_image_polygon(const Config& config);
			bool mark_as_custom();


		public:
			// void get_sliced_area(
			// 	SlicedArea area,
			// 	const Rect& guide,
			// 	Rect& xy,
			// 	RectUV& uv,
			// 	const Transformation xy_transform = Transformation()
			// ) const;

			void get_sliced_regions(
				const Rect& guide,
				const Transformation xy_transform,
				Container<Container<Vertex>>& result
			) const;

		public:
			bool operator ==(Item& other);

		private:
			void image_preprocess(const Config& config);
			void alpha_preprocess();

			void get_image_contour(cv::Mat& image, Container<cv::Point>& result);

			void normalize_mask(cv::Mat& mask);

		protected:
			Status m_status = Status::Unset;
			bool m_preprocessed = false;
			bool m_sliced = false;
			bool m_colorfill = false;

			cv::Mat m_image;
		};
	}
}