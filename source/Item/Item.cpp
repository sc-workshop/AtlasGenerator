#include "AtlasGenerator/Item/Item.h"

#define PercentOf(proc, num) (num * proc / 100)

namespace sc
{
	namespace AtlasGenerator
	{
		Item::Transformation::Transformation(double rotation, Point<int32_t> translation) : rotation(rotation), translation(translation)
		{
		}

		Item::Item(cv::Mat& image, Type type) : m_image(image), m_type(type)
		{
		}

		Item::Item(std::filesystem::path path, Type type) : m_type(type)
		{
			m_image = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
		}

		Item::Item(cv::Scalar color, Type type) : m_type(type)
		{
			m_image = cv::Mat(1, 1, CV_8UC4, color);
		}

		Item::Status Item::status() const { return m_status; }
		uint16_t Item::width() const { return (uint16_t)m_image.cols; };
		uint16_t Item::height() const { return (uint16_t)m_image.rows; };

		// TODO: move to self written image class?
		cv::Mat& Item::image() { return m_image; };

		bool Item::is_rectangle() const
		{
			if (is_sliced()) return true;

			return width() <= 10 || height() <= 10;
		};

		bool Item::is_sliced() const
		{
			return m_type == Item::Type::Sliced;
		}

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

			cv::Rect bound = boundingRect(alpha_mask);
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

				int x = point.x + bound.x;
				int y = point.y + bound.y;
				uint16_t u = (uint16_t)ceil(point.x / config.scale());
				uint16_t v = (uint16_t)ceil(point.y / config.scale());

				vertices.emplace_back(
					(uint16_t)x, (uint16_t)y,
					(uint16_t)u, (uint16_t)v
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

		Rect<int32_t> Item::bound() const
		{
			Rect<int32_t> result(INT_MAX, 0, 0, INT_MAX);

			for (const Vertex& vertex : vertices)
			{
				if (result.left > vertex.xy.x)
				{
					result.left = vertex.xy.x;
				}

				if (result.top < vertex.xy.y)
				{
					result.top = vertex.xy.y;
				}

				if (result.right < vertex.xy.x)
				{
					result.right = vertex.xy.x;
				}

				if (result.bottom > vertex.xy.y)
				{
					result.bottom = vertex.xy.y;
				}
			}

			return result;
		}

		void Item::get_sliced_area(Item::SlicedArea area, const Rect<int32_t>& guide, Rect<int32_t>& xy, Rect<uint16_t>& uv, const Transformation xy_transform) const
		{
			if (!is_rectangle()) return;

			// TODO: Move to seperate builder class?
			Rect<int32_t> xy_bound = bound();

			Point<int32_t> xy_bottom_left_corner(xy_bound.left, xy_bound.bottom);
			xy_transform.transform_point(xy_bottom_left_corner);

			Point<int32_t> xy_top_right_corner(xy_bound.right, xy_bound.top);
			xy_transform.transform_point(xy_top_right_corner);

			Rect<int32_t> xy_rectangle(
				xy_bottom_left_corner.x, xy_bottom_left_corner.y,
				xy_top_right_corner.x, xy_top_right_corner.y
			);

			Rect<uint16_t> uv_rectangle(
				vertices[0].uv.u, vertices[0].uv.v,
				vertices[2].uv.u, vertices[2].uv.v
			);

			int16_t left_width_size = (int16_t)abs(guide.right - xy_rectangle.x);
			int16_t top_height_size = (int16_t)abs(guide.top - xy_rectangle.height);
			int16_t bottom_height_size = (int16_t)abs(guide.bottom - xy_rectangle.y);
			int16_t middle_width_size = (int16_t)abs(guide.left - xy_rectangle.x) - left_width_size;
			int16_t middle_height_size = (int16_t)abs(guide.top - (xy_rectangle.y + bottom_height_size));
			int16_t right_width_size = (int16_t)abs(guide.left - xy_rectangle.width);

			switch (area)
			{
			case Item::SlicedArea::BottomLeft:
				if (guide.right < xy_rectangle.x || guide.bottom < xy_rectangle.y) return;

				{
					xy.x = xy_rectangle.x;
					xy.y = xy_rectangle.y;

					xy.width = left_width_size;
					xy.height = bottom_height_size;
				}

				{
					uv.x = uv_rectangle.x;
					uv.y = uv_rectangle.y;
				}
				break;
			case Item::SlicedArea::BottomMiddle:
			{
				xy.x = xy_rectangle.x + left_width_size;
				xy.y = xy_rectangle.y;

				xy.width = middle_width_size;
				xy.height = bottom_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size;
				uv.y = uv_rectangle.y;
			}

			break;
			case Item::SlicedArea::BottomRight:
			{
				xy.x = guide.left;
				xy.y = xy_rectangle.y;

				xy.width = right_width_size;
				xy.height = bottom_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size + middle_width_size;
				uv.y = uv_rectangle.y;
			}
			break;
			case Item::SlicedArea::MiddleLeft:
			{
				xy.x = xy_rectangle.x;
				xy.y = xy_rectangle.y + bottom_height_size;

				xy.width = left_width_size;
				xy.height = middle_height_size;
			}

			{
				uv.x = uv_rectangle.x;
				uv.y = uv_rectangle.y + bottom_height_size;
			}
			break;
			case Item::SlicedArea::Center:
			{
				xy.x = xy_rectangle.x + left_width_size;
				xy.y = xy_rectangle.y + bottom_height_size;

				xy.width = middle_width_size;
				xy.height = middle_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size;
				uv.y = uv_rectangle.y + bottom_height_size;
			}
			break;
			case Item::SlicedArea::MiddleRight:
			{
				xy.x = guide.left;
				xy.y = xy_rectangle.y + bottom_height_size;

				xy.width = right_width_size;
				xy.height = middle_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size + middle_width_size;
				uv.y = uv_rectangle.y + bottom_height_size;
			}
			break;
			case Item::SlicedArea::TopLeft:
			{
				xy.x = xy_rectangle.x;
				xy.y = xy_rectangle.y + bottom_height_size + middle_height_size;

				xy.width = left_width_size;
				xy.height = top_height_size;
			}

			{
				uv.x = uv_rectangle.x;
				uv.y = uv_rectangle.y + bottom_height_size + middle_height_size;
			}
			break;
			case Item::SlicedArea::TopMiddle:
			{
				xy.x = xy_rectangle.x + left_width_size;
				xy.y = xy_rectangle.y + bottom_height_size + middle_height_size;

				xy.width = middle_width_size;
				xy.height = top_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size;
				uv.y = uv_rectangle.y + bottom_height_size + middle_height_size;
			}
			break;
			case Item::SlicedArea::TopRight:
			{
				xy.x = guide.left;
				xy.y = xy_rectangle.y + bottom_height_size + middle_height_size;

				xy.width = right_width_size;
				xy.height = top_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size + middle_width_size;
				uv.y = uv_rectangle.y + bottom_height_size + middle_height_size;
			}
			break;
			default:
				break;
			}

			uv.width = (uint16_t)std::clamp<int32_t>(xy.width, 0i16, UINT16_MAX);
			uv.height = (uint16_t)std::clamp<int32_t>(xy.height, 0i16, UINT16_MAX);
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

						if (pixel[3] <= 2) {
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
						Vec2b& pixel = m_image.at<Vec2b>(h, w);

						if (pixel[1] <= 2) {
							pixel[0] = 0;
							pixel[1] = 0;
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
				cv::Mat drawingImage;
				cvtColor(image, drawingImage, cv::COLOR_GRAY2BGR);
				ShowContour(drawingImage, points);
#endif

				std::move(points.begin(), points.end(), std::back_inserter(result));
			}

			snap_to_border(image, result);
		}

		void Item::mask_preprocess(cv::Mat& mask)
		{
			using namespace cv;

			// Pixel Normalize
			for (uint16_t h = 0; mask.cols > h; h++) {
				for (uint16_t w = 0; mask.rows > w; w++) {
					uchar* pixel = mask.ptr() + (h * w);

					if (*pixel >= 1) {
						*pixel = 255;
					}
					else
					{
						*pixel = 0;
					};
				}
			}

			// Mask blur
			//Mat blurred;
			//const double sigma = 5, amount = 2.5;
			//
			//GaussianBlur(mask, blurred, Size(), sigma, sigma);
			//mask = mask * (1 + amount) + blurred * (-amount);
#ifdef CV_DEBUG
			ShowImage("Mask", mask);
#endif // CV_DEBUG
		}

		void Item::snap_to_border(cv::Mat& src, Container<cv::Point>& points)
		{
			using namespace cv;

			const double snapPercent = 7;

			// Snaping variables
			const int minW = (int)ceil(PercentOf(src.cols, snapPercent));
			const int maxW = src.cols - minW;

			const int minH = (int)ceil(PercentOf(src.rows, snapPercent));
			const int maxH = src.rows - minH;

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