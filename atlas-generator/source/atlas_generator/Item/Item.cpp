#include "atlas_generator/Item/Item.h"

#include "core/math/triangle.h"
#include "core/geometry/intersect.hpp"

#include <cmath>
#include <clipper2/clipper.h>

namespace wk
{
	namespace AtlasGenerator
	{
		Item::Transformation::Transformation(double rotation, Point translation) : rotation(rotation), translation(translation)
		{
		}

		Item::Item(cv::Mat& image, bool sliced) : m_sliced(sliced), m_image(image)
		{
		}

		Item::Item(cv::Scalar color) : m_colorfill(true)
		{
			m_image = cv::Mat(1, 1, CV_8UC4, color);
		}

#ifdef ATLAS_GENERATOR_WITH_IMAGE_CODECS
		Item::Item(std::filesystem::path path, bool sliced) :
			m_sliced(sliced)
		{
			m_image = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
		}
#endif

		Item::Status Item::status() const { return m_status; }
		uint16_t Item::width() const { return (uint16_t)image().cols; };
		uint16_t Item::height() const { return (uint16_t)image().rows; };

		bool Item::is_rectangle() const
		{
			if (is_sliced()) return true;

			return width() <= 50 || height() <= 50;
		};

		bool Item::is_sliced() const
		{
			return m_sliced;
		}

		std::optional<AtlasGenerator::Vertex> Item::get_colorfill() const
		{
			if (vertices.size() >= 1 && m_colorfill)
			{
				return vertices[0];
			}

			return std::nullopt;
		}

		bool Item::mark_as_custom()
		{
			m_status = Status::Valid;
			return true;
		}

		void Item::generate_image_polygon(const Config& config)
		{
			using namespace cv;
			using namespace wk::Geometry;

			float scale_factor = is_sliced() ? 1.0f : config.scale();
			Size full_size(m_image.cols, m_image.rows);
			PointF crop_offset(0.0f, 0.0f);
			Size current_size = full_size;

			auto fallback_rectangle = [this, &crop_offset, &full_size, &current_size]
				{
					vertices.resize(4);

					vertices[3].uv = { 0,												0 };
					vertices[2].uv = { 0,												(uint16_t)current_size.height };
					vertices[1].uv = { (uint16_t)current_size.width,					(uint16_t)current_size.height };
					vertices[0].uv = { (uint16_t)current_size.width,					0 };

					vertices[3].xy = { (uint16_t)crop_offset.x,								(uint16_t)crop_offset.y };
					vertices[2].xy = { (uint16_t)crop_offset.x,								(uint16_t)(crop_offset.y + current_size.height) };
					vertices[1].xy = { (uint16_t)(crop_offset.x + current_size.width),		(uint16_t)(crop_offset.y + current_size.height) };
					vertices[0].xy = { (uint16_t)(crop_offset.x + current_size.width),		(uint16_t)crop_offset.y };

					m_status = Status::Valid;
				};
			image_preprocess(config);

			if (1 >= width() || 1 >= height())
			{
				fallback_rectangle();
				return;
			}

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
				fallback_rectangle();
				return;
			}

			cv::Rect crop_bound = boundingRect(alpha_mask);
			if (crop_bound.width <= 0) crop_bound.width = 1;
			if (crop_bound.height <= 0) crop_bound.height = 1;

			// Image cropping by alpha
			m_image = m_image(crop_bound);
			alpha_mask = alpha_mask(crop_bound);

			current_size = alpha_mask.size();
			crop_offset = PointF(
				crop_bound.x * scale_factor,
				crop_bound.y * scale_factor
			);

			if (is_rectangle()) {
				fallback_rectangle();
				return;
			}
			else {
				vertices.clear();
				vertices.reserve(8);
			}

			normalize_mask(alpha_mask, config);

			Container<Point> polygon;
			{
				Container<cv::Point> hull;
				{
					Container<cv::Point> contour;
					get_image_contour(alpha_mask, contour);

					//ShowContour(alpha_mask, contour);

					// Getting convex hull as base polygon for calculations
					convexHull(contour, hull, true);
				}

				polygon.reserve(hull.size());
				for (cv::Point& point : hull)
				{
					polygon.emplace_back(point.x, point.y);
				}
			}

			Container<Triangle> triangles;
			triangles.reserve(4);

			Point centroid = {
				static_cast<int>(abs(static_cast<float>(current_size.width) / 2)),
				static_cast<int>(abs(static_cast<float>(current_size.height) / 2))
			};

			float distance_threshold = (current_size.width + current_size.height) * 0.025f;

			auto calculate_triangle = [&centroid, &current_size, &polygon, &triangles, &distance_threshold, this](Point input_point)
				{
					LineF ray(
						PointF((float)input_point.x, (float)input_point.y),
						PointF((float)centroid.x, (float)centroid.y)
					);

					auto intersection_result = line_intersect(polygon, ray);
					if (!intersection_result.has_value()) return;

					auto& [p1_idx, p2_idx, intersect] = intersection_result.value();

					const Point& p1 = polygon[p1_idx];
					const Point& p2 = polygon[p2_idx];

					// Skip processing if distance between corner and intersect point is too smol
					{
						float distance = dist(input_point, intersect);

						if (distance_threshold > distance)
						{
							return;
						}
					}

					float angle = line_angle(Line(p1, p2));

					Line cutoff_bisector = Line(
						input_point,
						Point((int)ceil(intersect.x), (int)ceil(intersect.y))
					);

					Triangle cutoff = build_triangle(
						cutoff_bisector, angle, (current_size.width + current_size.height) * 2
					);

					triangles.push_back(cutoff);
				};

			Rect bounding_box = { 0, 0, current_size.width, current_size.height };

			calculate_triangle(Point(0, 0));
			calculate_triangle(Point(current_size.width, 0));
			calculate_triangle(Point(current_size.width, current_size.height));
			calculate_triangle(Point(0, current_size.height));

			if (triangles.empty())
			{
				fallback_rectangle();
				return;
			}

			// Using all calculated triangles to cut polygon
			{
				using namespace Clipper2Lib;

				PathsD clip, solution;

				// Polygon
				PathD subject;
				subject.reserve(4);
				subject.emplace_back(0, 0);
				subject.emplace_back(current_size.width, 0);
				subject.emplace_back(current_size.width, current_size.height);
				subject.emplace_back(0, current_size.height);

				// Triangles
				clip.reserve(triangles.size());
				for (Triangle& triangle : triangles)
				{
					PathD& path = clip.emplace_back();
					path.reserve(3);

					path.emplace_back((double)triangle.p1.x, (double)triangle.p1.y);
					path.emplace_back((double)triangle.p2.x, (double)triangle.p2.y);
					path.emplace_back((double)triangle.p3.x, (double)triangle.p3.y);
				}

				solution = Difference(PathsD({ subject }), clip, FillRule::NonZero);

				if (solution.size() != 1)
				{
#ifdef CV_DEBUG
					for (size_t i = 0; solution.size() > i; i++)
					{
						PathD& path = solution[i];
						std::vector<cv::Point> points;
						for (auto& point : path)
						{
							points.emplace_back(point.x, point.y);
						}

						ShowContour(m_image, points);

						for (auto& triangle : triangles)
						{
							ShowContour(m_image, Container<Point>{ triangle.p1, triangle.p2, triangle.p3 });
						}
					}
#endif

					assert(0);
					fallback_rectangle();
					return;
				}

				PathD& path = solution[0];
				vertices.reserve(path.size());
				for (auto& point : path)
				{
					int32_t x = (int32_t)std::ceil((point.x + crop_bound.x) * scale_factor);
					int32_t y = (int32_t)std::ceil((point.y + crop_bound.y) * scale_factor);
					uint16_t u = (uint16_t)point.x;
					uint16_t v = (uint16_t)point.y;

					vertices.emplace_back(x, y, u, v);
				}
			}

			if (vertices.empty())
			{
				fallback_rectangle();
			}
			else
			{
				m_status = Status::Valid;
			}
		};

		Rect Item::bound() const
		{
			Rect result(
				std::numeric_limits<int>::max(), 0,
				0, std::numeric_limits<int>::max()
			);

			for (const Vertex& vertex : vertices)
			{
				result.left = std::min(result.left, vertex.xy.x);
				result.bottom = std::min(result.bottom, vertex.xy.y);
				result.right = std::max(result.right, vertex.xy.x);
				result.top = std::max(result.top, vertex.xy.y);
			}

			return result;
		}

		RectUV Item::bound_uv() const
		{
			RectUV result(
				std::numeric_limits<uint16_t>::max(), 0,
				0, std::numeric_limits<uint16_t>::max()
			);

			for (const Vertex& vertex : vertices)
			{
				result.left = std::min(result.left, vertex.uv.x);
				result.bottom = std::min(result.bottom, vertex.uv.y);
				result.right = std::max(result.right, vertex.uv.x);
				result.top = std::max(result.top, vertex.uv.y);
			}

			return result;
		}

		void Item::get_9slice(
			const Rect& guide,
			Container<Container<Vertex>>& result,
			const Transformation xy_transform
		) const
		{
			using namespace Clipper2Lib;

			if (!is_rectangle()) return;

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

				uv_size.x = std::abs(uv_bound.left - uv_bound.right);
				uv_size.y = std::abs(uv_bound.top - uv_bound.bottom);
			}

			PathsD result_solution;
			{
				PathD subject;

				subject.reserve(vertices.size());
				for (const Vertex& vertex : vertices)
				{
					subject.emplace_back(
						vertex.xy.x + xy_transform.translation.x,
						vertex.xy.y + xy_transform.translation.y
					);
				}

				constexpr int min = std::numeric_limits<int>::min();
				constexpr int max = std::numeric_limits<int>::max();

				const Container<Rect> rects = {
					{min, min, guide.left, guide.bottom},															// Left-Top
					{min, guide.bottom, guide.left, guide.top},														// Top-Middle
					{guide.left, guide.top, min, max},																// Right-Top

					{guide.left, min, guide.right, guide.bottom},													// Left-Middle
					{guide.left, guide.bottom, guide.right, guide.top},												// Middle
					{guide.left, guide.top, guide.right, max},														// Middle-bottom

					{guide.right, guide.bottom, max, min },															// Left-bottom
					{guide.right, guide.top, max, guide.bottom},													// Middle-bottom
					{guide.right, guide.top, max, max},																// Right-bottom

				};

				for (const Rect& rect : rects)
				{
					PathD path;

					path.emplace_back(rect.bottom, rect.left);
					path.emplace_back(rect.bottom, rect.right);
					path.emplace_back(rect.top, rect.right);
					path.emplace_back(rect.top, rect.left);

					PathsD solution = Intersect({ subject }, { path }, FillRule::NonZero);
					result_solution.insert(result_solution.end(), solution.begin(), solution.end());
				}
			}

			for (PathD& path : result_solution)
			{
				Container<Vertex>& result_path = result.emplace_back();
				for (const PointD& path_vertex : path)
				{
					Vertex& vertex = result_path.emplace_back();

					vertex.xy.x = (int)path_vertex.x;
					vertex.xy.y = (int)path_vertex.y;

					// Remap xy to uv map

					// normalize to 0.0...1.0 range
					PointF uv_coord(
						(path_vertex.x - offset.x) / size.x,
						(path_vertex.y - offset.y) / size.y
					);

					vertex.uv.x = uv_coord.x * uv_size.x;
					vertex.uv.y = uv_coord.y * uv_size.y;
				}
			}
		}

		bool Item::operator ==(const Item& other) const
		{
			using namespace cv;

			if (std::addressof(image()) == std::addressof(other.image())) return true;

			if (m_image.type() != other.image().type() || width() != other.width() || height() != other.height()) return false;
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

		void Item::image_preprocess(const Config& config)
		{
			if (m_preprocessed) return;

			if (config.scale() != 1.0f && !is_sliced())
			{
				cv::Size sprite_size(
					(int)ceil(m_image.cols / config.scale()),
					(int)ceil(m_image.rows / config.scale())
				);

				cv::resize(m_image, m_image, sprite_size);
			}

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

						float alpha = static_cast<float>(pixel[1]) / 255.0f;
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
			for (uint16_t h = 0; image.cols > h; h++) {
				for (uint16_t w = 0; image.rows > w; w++) {
					uchar& pixel = image.at<uchar>(w, h);
					bool valid = false;

					// Iterate over black pixels only
					if (pixel > 1)
					{
						if (h == 0 || w == 0 || h == image.cols - 1 || w == image.rows - 1)
						{
							result.emplace_back(h, w);
							continue;
						}
					}
					else
					{
						continue;
					};


					bool has_positive = false;
					bool has_negative = false;
					for (int dy = -1; dy <= 1; dy++) {
						for (int dx = -1; dx <= 1; dx++) {
							int x = w + dy;
							int y = h + dx;

							if (dx == 0 && dy == 0) continue;
							if (0 > x || 0 > y) continue;
							if (x >= image.rows || y >= image.cols) continue;

							uchar& neighbor = image.at<uchar>(x, y);

							has_positive = has_positive || neighbor > 0;
							has_negative = has_negative || neighbor == 0;
						}
					}

					valid = has_negative && has_positive;
					if (valid)
					{
						result.emplace_back(h, w);
					}

				}
			}
		}

		void Item::normalize_mask(cv::Mat& mask, const Config& config)
		{
			using namespace cv;

			// Pixel Normalize
			for (uint16_t h = 0; mask.cols > h; h++) {
				for (uint16_t w = 0; mask.rows > w; w++) {
					uchar& pixel = mask.at<uchar>(w, h);

					if (pixel > config.alpha_threshold()) {
						pixel = 255;
					}
					else
					{
						pixel = 0;
					};
				}
			}

			const int size = 2;
			cv::Mat element = cv::getStructuringElement(cv::MORPH_CROSS,
				cv::Size(2 * size + 1, 2 * size + 1),
				cv::Point(size, size));
			cv::dilate(mask, mask, element);
		}
	}
}