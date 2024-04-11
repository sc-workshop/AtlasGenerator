#pragma once

#include <stdint.h>
#include <vector>
#include <opencv2/opencv.hpp>
#include <filesystem>

#include "Vertex.h"
#include "AtlasGenerator/Config.h"

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
			enum class Status
			{
				Unset = 0,
				Valid,
				InvalidPolygon
			};

		public:
			Item(cv::Mat& image);
			Item(std::filesystem::path path);

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

		public:
			bool is_rectangle();

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
			bool m_preprocessed = true;
			cv::Mat m_image;
		};
	}
}