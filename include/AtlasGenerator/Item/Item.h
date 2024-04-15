#pragma once

#include <stdint.h>
#include <vector>
#include <opencv2/opencv.hpp>
#include <filesystem>

#include "Vertex.h"
#include "AtlasGenerator/Config.h"

#ifdef CV_DEBUG
void ShowContour(cv::Mat& src, std::vector<cv::Point>& points);
void ShowImage(std::string name, cv::Mat& image);
#endif

namespace sc
{
	namespace AtlasGenerator
	{
		template<typename T>
		using Container = std::vector<T>;

		// Interface that represents Atlas Sprite
		class Item
		{
		public:
			struct Transformation
			{
				// Rotation in radians
				double rotation = 0.0;
				Point<int32_t> translation = Point<int32_t>(0, 0);

				template<typename T>
				void transform_point(Point<T>& vertex)
				{
					T x = vertex.x;
					T y = vertex.y;

					vertex.x = (T)ceil(x * std::cos(rotation) - y * std::sin(rotation) + translation.x);
					vertex.y = (T)ceil(y * std::cos(rotation) + x * std::sin(rotation) + translation.y);
				}
			};
		public:
			enum class Status
			{
				Unset = 0,
				Valid,
				InvalidPolygon
			};

		public:
			Item(cv::Mat& image);
			Item(std::filesystem::path path);
			Item(cv::Scalar color);

			virtual ~Item() = default;

			// Image Info
		public:
			virtual Status status() const;
			virtual uint16_t width() const;
			virtual uint16_t height() const;

			// TODO: move to self written image class?
			virtual cv::Mat& image();

			// Generator Info
		public:
			uint8_t texture_index = 0xFF;
			Container<Vertex> vertices;

			// UV Transformation
			Transformation transform;

		public:
			virtual bool is_rectangle() const;
			virtual bool is_sliced() const;

		public:
			// XY coords bound
			Rect<int32_t> bound();
			void generate_image_polygon(const Config& config);
			float perimeter() const;

		public:
			bool operator ==(Item& other);

		private:
			void image_preprocess();
			void alpha_preprocess();

			void get_image_contour(cv::Mat& image, Container<cv::Point>& result);

			void mask_preprocess(cv::Mat& mask);

			void snap_to_border(cv::Mat& src, Container<cv::Point>& points);
			void extrude_points(cv::Mat& src, Container<cv::Point>& points);

		protected:
			Status m_status = Status::Unset;
			bool m_preprocessed = false;
			cv::Mat m_image;
		};
	}
}