#include "AtlasGenerator/Item/Item.h"

#define PercentOf(proc, num) (num * proc / 100)

namespace sc
{
	namespace AtlasGenerator
	{
		Item::Item(cv::Mat& image) : m_image(image)
		{
		}

		Item::Item(std::filesystem::path path)
		{
			m_image = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
		}

		Item::Status Item::status() const { return m_status; }
		uint16_t Item::width() const { return m_image.cols; };
		uint16_t Item::height() const { return m_image.rows; };

		// TODO: move to self written image class?
		cv::Mat& Item::image() { return m_image; };

		bool Item::is_rectangle()
		{
			return width() <= 10 || height() <= 10;
		};

		void Item::generate_image_polygon(const Config& config)
		{
			using namespace cv;

			image_preprocess();

			Mat alpha_mask;
			switch (m_image.channels())
			{
			case 4:
				extractChannel(m_image, alpha_mask, 3);
				break;
			case 2:
				extractChannel(m_image, alpha_mask, 1);
				break;
			default:
				alpha_mask = Mat(m_image.size(), CV_8UC1, Scalar(255));
				break;
			}

			Rect bound = boundingRect(alpha_mask);
			if (bound.width <= 0) bound.width = 1;
			if (bound.height <= 0) bound.height = 1;

			// Image croping by alpha
			m_image = m_image(bound);
			alpha_mask = alpha_mask(bound);

			Size dstSize = alpha_mask.size();

			if (is_rectangle()) {
				vertices.resize(4);

				vertices[0].uv = { 0,						0 };
				vertices[1].uv = { 0,						(uint16_t)dstSize.height };
				vertices[2].uv = { (uint16_t)dstSize.width, (uint16_t)dstSize.height };
				vertices[3].uv = { (uint16_t)dstSize.width, 0 };

				vertices[0].xy = { (uint16_t)bound.x,					  (uint16_t)bound.y };
				vertices[1].xy = { (uint16_t)bound.x,					  (uint16_t)(bound.y + dstSize.height) };
				vertices[2].xy = { (uint16_t)(bound.x + dstSize.width), (uint16_t)(bound.y + dstSize.height) };
				vertices[3].xy = { (uint16_t)(bound.x + dstSize.width), (uint16_t)bound.y };

				m_status = Status::Valid;
				return;
			}
			else {
				vertices.clear();
			}

			mask_preprocess(alpha_mask);

			Container<cv::Point> contour;
			get_image_contour(alpha_mask, contour);

			Container<cv::Point> polygon;
			extrude_points(alpha_mask, polygon);
			convexHull(contour, polygon, true);

			for (uint32_t i = 0; polygon.size() > i; i++)
			{
				const cv::Point& point = polygon[i];

				uint16_t x = point.x + bound.x;
				uint16_t y = point.y + bound.y;
				uint16_t u = (uint16_t)ceil(point.x / config.scale());
				uint16_t v = (uint16_t)ceil(point.y / config.scale());

				vertices.emplace_back(
					x, y,
					u, v
				);
			}

			if (vertices.empty())
			{
				m_status = Status::InvalidPolygon;
			}
			else
			{
				m_status = Status::Valid;
			}
		};

		float Item::perimeter() const
		{
			float result = 0;
			for (size_t i = 0; vertices.size() > i; i++)
			{
				if ((i % 2) == 0) continue;

				bool isEndLine = i == vertices.size() - 1;

				const Vertex& pointStart = vertices[i];
				const Vertex& pointEnd = vertices[isEndLine ? 0 : i - 1];

				float distance = std::sqrt(
					powf((float)(pointStart.uv.y - pointStart.uv.x), 2.0f) +
					powf((float)(pointEnd.uv.y - pointEnd.uv.x), 2.0f)
				);
				result += distance;
			}

			return result;
		}

		bool Item::operator ==(Item& other)
		{
			using namespace cv;

			if (width() != other.width() || height() != other.height()) return false;
			int imageChannelsCount = other.image().channels();
			int otherChannelsCount = other.image().channels();

			if (imageChannelsCount != otherChannelsCount) return false;

			std::vector<Mat> channels(imageChannelsCount);
			std::vector<Mat> otherChannels(imageChannelsCount);
			split(image(), channels);
			split(other.image(), otherChannels);

			size_t pixelCount = width() * height();
			for (int j = 0; imageChannelsCount > j; j++) {
				for (int w = 0; width() > w; w++) {
					for (int h = 0; height() > h; h++) {
						uchar pix = channels[j].at<uchar>(h, w);
						uchar otherPix = otherChannels[j].at<uchar>(h, w);
						if (pix != otherPix) {
							return false;
						}
					}
				}
			}

			return true;
		}

		void Item::image_preprocess()
		{
			if (m_preprocessed) return;

			int channels = m_image.channels();

			if (channels == 2 || channels == 4) {
				alpha_preprocess();
			}

			m_preprocessed = true;
		}
		void Item::alpha_preprocess()
		{
			using namespace cv;

			int channels = m_image.channels();

			for (uint16_t h = 0; height() > h; h++) {
				for (uint16_t w = 0; width() > w; w++) {
					switch (channels)
					{
					case 4:
					{
						Vec4b pixel = m_image.at<Vec4b>(h, w);

						if (pixel[3] < 4) {
							m_image.at<cv::Vec4b>(h, w) = { 0, 0, 0, 0 };
							continue;
						};

						// Alpha premultiply

						float alpha = static_cast<float>(pixel[3]) / 255.0f;
						m_image.at<Vec4b>(h, w) = {
							static_cast<uchar>(pixel[0] * alpha),
							static_cast<uchar>(pixel[1] * alpha),
							static_cast<uchar>(pixel[2] * alpha),
							pixel[3]
						};
					}
					break;
					case 2:
					{
						Vec2b pixel = m_image.at<Vec2b>(h, w);

						if (pixel[1] < 4) {
							m_image.at<cv::Vec2b>(h, w) = { 0, 0 };
							continue;
						};
						float alpha = static_cast<float>(pixel[3]) / 255.0f;
						m_image.at<Vec2b>(h, w) = {
							static_cast<uchar>(pixel[0] * alpha),
							pixel[1]
						};
					}
					break;
					}
				}
			}
		}

		void Item::get_image_contour(cv::Mat& image, Container<cv::Point>& result)
		{
			Container<Container<cv::Point>> contours;
			findContours(image, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

			for (std::vector<cv::Point>& points : contours) {
#ifdef CV_DEBUG
				Mat drawingImage;
				cvtColor(image, drawingImage, COLOR_GRAY2BGR);
				ShowContour(drawingImage, points);
#endif

				std::move(points.begin(), points.end(), std::back_inserter(result));
			}

			snap_to_border(image, result);
		}

		void Item::mask_preprocess(cv::Mat& mask)
		{
			using namespace cv;

			Mat blurred;
			const double sigma = 5, amount = 2.5;

			GaussianBlur(mask, blurred, Size(), sigma, sigma);
			mask = mask * (1 + amount) + blurred * (-amount);
#ifdef CV_DEBUG
			ShowImage("Mask", sharpened);
#endif // CV_DEBUG
		}

		void Item::snap_to_border(cv::Mat& src, Container<cv::Point>& points)
		{
			using namespace cv;

			const double snapPercent = 7;

			// Snaping variables
			const uint16_t minW = (uint16_t)ceil(PercentOf(src.cols, snapPercent));
			const uint16_t maxW = src.cols - minW;

			const uint16_t minH = (uint16_t)ceil(PercentOf(src.rows, snapPercent));
			const uint16_t maxH = src.rows - minH;

			for (cv::Point& point : points) {
				if (minW > point.x) {
					point.x = 0;
				}
				else if (point.x >= maxW) {
					point.x = src.cols;
				}

				if (minH > point.y) {
					point.y = 0;
				}
				else if (point.y >= maxH) {
					point.y = src.rows;
				}
			}
		}

		void Item::extrude_points(cv::Mat& src, Container<cv::Point>& points)
		{
			using namespace cv;

			const uint16_t offsetX = (uint16_t)PercentOf(src.cols, 5);
			const uint16_t offsetY = (uint16_t)PercentOf(src.rows, 5);

			const int centerW = (int)ceil(src.cols / 2);
			const int centerH = (int)ceil(src.rows / 2);

			for (cv::Point& point : points) {
				bool is_edge_point = (point.x == 0 || point.x == src.cols) && (point.y == 0 || point.y == src.rows);

				if (!is_edge_point) {
					int x = point.x - centerW;
					int y = point.y - centerH;

					if (x >= 0) {
						x += offsetX;
					}
					else {
						x -= offsetX;
					}

					if (y >= 0) {
						y += offsetY;
					}
					else {
						y -= offsetY;
					}

					point = {
						std::clamp(x + centerW, 0, src.cols),
						std::clamp(y + centerH, 0, src.rows),
					};
				}
			}
		}
	}
}