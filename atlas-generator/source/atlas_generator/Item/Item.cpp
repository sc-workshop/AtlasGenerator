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

			image_preprocess(config);

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

			cv::Rect crop_bound = boundingRect(alpha_mask);
			if (crop_bound.width <= 0) crop_bound.width = 1;
			if (crop_bound.height <= 0) crop_bound.height = 1;

			// Image cropping by alpha
			m_image = m_image(crop_bound);
			alpha_mask = alpha_mask(crop_bound);

			Size current_size = alpha_mask.size();

			PointF crop_offset(
				crop_bound.x * scale_factor,
				crop_bound.y * scale_factor
			);

			auto fallback_rectangle = [this, &crop_offset, &full_size, &current_size]
				{
					vertices.resize(4);

					vertices[0].uv = { 0,										0 };
					vertices[1].uv = { 0,										(uint16_t)current_size.height };
					vertices[2].uv = { (uint16_t)current_size.width,					(uint16_t)current_size.height };
					vertices[3].uv = { (uint16_t)current_size.width,					0 };

					vertices[0].xy = { (uint16_t)crop_offset.x,						(uint16_t)crop_offset.y };
					vertices[1].xy = { (uint16_t)crop_offset.x,						(uint16_t)(crop_offset.y + full_size.height) };
					vertices[2].xy = { (uint16_t)(crop_offset.x + full_size.width),		(uint16_t)(crop_offset.y + full_size.height) };
					vertices[3].xy = { (uint16_t)(crop_offset.x + full_size.width),		(uint16_t)crop_offset.y };

					m_status = Status::Valid;
				};

			if (is_rectangle()) {
				fallback_rectangle();
				return;
			}
			else {
				vertices.clear();
				vertices.reserve(8);
			}

			normalize_mask(alpha_mask);

			Container<Point> polygon;
			{
				Container<cv::Point> contour;
				get_image_contour(alpha_mask, contour);

				// Contour area length
				//double area = arcLength(contour, true);

				// Simplify contour just a bit for better results
				//approxPolyDP(contour, contour, 0.0009 * area, true);

				// Getting convex hull as base polygon for calculations
				Container<cv::Point> hull;
				convexHull(contour, hull, true);

				polygon.reserve(contour.size());
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

					//ShowContour(m_image, Container<Point>{ input_point, centroid });

					auto intersection_result = line_intersect(polygon, ray);
					if (!intersection_result.has_value()) return;

					auto& [p1_idx, p2_idx, intersect] = intersection_result.value();

					const Point& p1 = polygon[p1_idx];
					const Point& p2 = polygon[p2_idx];

					//ShowContour(m_image, Container<Point>{ p1, p2 });

					// Skip processing if distance between corner and intersect point is too smol
					{
						float distance = dist(input_point, intersect);// std::hypot(p1.x - p2.x, p1.y - p2.y);

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
						cutoff_bisector, angle, current_size.width + current_size.height
					);

					//ShowContour(m_image, Container<Point>{ cutoff.p1, cutoff.p2, cutoff.p3 });

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
					// for (size_t i = 0; solution.size() > i; i++)
					// {
					// 	PathD& path = solution[i];
					// 	std::vector<cv::Point> points;
					// 	for (auto& point : path)
					// 	{
					// 		points.emplace_back(point.x, point.y);
					// 	}
					// 
					// 	ShowContour(m_image, points);
					// 
					// 	for (auto& triangle : triangles)
					// 	{
					// 		ShowContour(m_image, Container<Point>{ triangle.p1, triangle.p2, triangle.p3 });
					// 	}
					// }

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
			Rect result(INT_MAX, INT_MAX, 0, 0);

			for (const Vertex& vertex : vertices)
			{
				result.left = std::min(result.left, vertex.xy.x);
				result.bottom = std::min(result.bottom, vertex.xy.y);
				result.right = std::max(result.right, vertex.xy.x);
				result.top = std::max(result.top, vertex.xy.y);
			}

			return result;
		}

		//void Item::get_sliced_area(Item::SlicedArea area, const Rect& guide, Rect& xy, RectUV& uv, const Transformation xy_transform) const
		//{
		//	if (!is_rectangle()) return;
		//
		//	// TODO: Move to separate builder class?
		//	Rect xy_bound = bound();
		//
		//	Point xy_bottom_left_corner(xy_bound.left, xy_bound.bottom);
		//	xy_transform.transform_point(xy_bottom_left_corner);
		//
		//	Point xy_top_right_corner(xy_bound.right, xy_bound.top);
		//	xy_transform.transform_point(xy_top_right_corner);
		//
		//	Rect xy_rectangle(
		//		xy_bottom_left_corner.x, xy_bottom_left_corner.y,
		//		xy_top_right_corner.x, xy_top_right_corner.y
		//	);
		//
		//	RectUV uv_rectangle(
		//		vertices[0].uv.u, vertices[0].uv.v,
		//		vertices[2].uv.u, vertices[2].uv.v
		//	);
		//
		//	int16_t left_width_size = (int16_t)abs(guide.right - xy_rectangle.x);
		//	int16_t top_height_size = (int16_t)abs(guide.top - xy_rectangle.height);
		//	int16_t bottom_height_size = (int16_t)abs(guide.bottom - xy_rectangle.y);
		//	int16_t middle_width_size = (int16_t)abs((guide.left - xy_rectangle.x) - left_width_size);
		//	int16_t middle_height_size = (int16_t)abs(guide.top - (xy_rectangle.y + bottom_height_size));
		//	int16_t right_width_size = (int16_t)abs(guide.left - xy_rectangle.width);
		//
		//	switch (area)
		//	{
		//	case Item::SlicedArea::BottomLeft:
		//		if (guide.right < xy_rectangle.x || guide.bottom < xy_rectangle.y) return;
		//
		//		{
		//			xy.x = xy_rectangle.x;
		//			xy.y = xy_rectangle.y;
		//
		//			xy.width = left_width_size;
		//			xy.height = bottom_height_size;
		//		}
		//
		//		{
		//			uv.x = uv_rectangle.x;
		//			uv.y = uv_rectangle.y;
		//		}
		//		break;
		//	case Item::SlicedArea::BottomMiddle:
		//	{
		//		xy.x = xy_rectangle.x + left_width_size;
		//		xy.y = xy_rectangle.y;
		//
		//		xy.width = middle_width_size;
		//		xy.height = bottom_height_size;
		//	}
		//
		//	{
		//		uv.x = uv_rectangle.x + left_width_size;
		//		uv.y = uv_rectangle.y;
		//	}
		//
		//	break;
		//	case Item::SlicedArea::BottomRight:
		//	{
		//		xy.x = guide.left;
		//		xy.y = xy_rectangle.y;
		//
		//		xy.width = right_width_size;
		//		xy.height = bottom_height_size;
		//	}
		//
		//	{
		//		uv.x = uv_rectangle.x + left_width_size + middle_width_size;
		//		uv.y = uv_rectangle.y;
		//	}
		//	break;
		//	case Item::SlicedArea::MiddleLeft:
		//	{
		//		xy.x = xy_rectangle.x;
		//		xy.y = xy_rectangle.y + bottom_height_size;
		//
		//		xy.width = left_width_size;
		//		xy.height = middle_height_size;
		//	}
		//
		//	{
		//		uv.x = uv_rectangle.x;
		//		uv.y = uv_rectangle.y + bottom_height_size;
		//	}
		//	break;
		//	case Item::SlicedArea::Center:
		//	{
		//		xy.x = xy_rectangle.x + left_width_size;
		//		xy.y = xy_rectangle.y + bottom_height_size;
		//
		//		xy.width = middle_width_size;
		//		xy.height = middle_height_size;
		//	}
		//
		//	{
		//		uv.x = uv_rectangle.x + left_width_size;
		//		uv.y = uv_rectangle.y + bottom_height_size;
		//	}
		//	break;
		//	case Item::SlicedArea::MiddleRight:
		//	{
		//		xy.x = guide.left;
		//		xy.y = xy_rectangle.y + bottom_height_size;
		//
		//		xy.width = right_width_size;
		//		xy.height = middle_height_size;
		//	}
		//
		//	{
		//		uv.x = uv_rectangle.x + left_width_size + middle_width_size;
		//		uv.y = uv_rectangle.y + bottom_height_size;
		//	}
		//	break;
		//	case Item::SlicedArea::TopLeft:
		//	{
		//		xy.x = xy_rectangle.x;
		//		xy.y = xy_rectangle.y + bottom_height_size + middle_height_size;
		//
		//		xy.width = left_width_size;
		//		xy.height = top_height_size;
		//	}
		//
		//	{
		//		uv.x = uv_rectangle.x;
		//		uv.y = uv_rectangle.y + bottom_height_size + middle_height_size;
		//	}
		//	break;
		//	case Item::SlicedArea::TopMiddle:
		//	{
		//		xy.x = xy_rectangle.x + left_width_size;
		//		xy.y = xy_rectangle.y + bottom_height_size + middle_height_size;
		//
		//		xy.width = middle_width_size;
		//		xy.height = top_height_size;
		//	}
		//
		//	{
		//		uv.x = uv_rectangle.x + left_width_size;
		//		uv.y = uv_rectangle.y + bottom_height_size + middle_height_size;
		//	}
		//	break;
		//	case Item::SlicedArea::TopRight:
		//	{
		//		xy.x = guide.left;
		//		xy.y = xy_rectangle.y + bottom_height_size + middle_height_size;
		//
		//		xy.width = right_width_size;
		//		xy.height = top_height_size;
		//	}
		//
		//	{
		//		uv.x = uv_rectangle.x + left_width_size + middle_width_size;
		//		uv.y = uv_rectangle.y + bottom_height_size + middle_height_size;
		//	}
		//	break;
		//	default:
		//		break;
		//	}
		//
		//	uv.width = (uint16_t)std::clamp<int32_t>(xy.width, 0i16, UINT16_MAX);
		//	uv.height = (uint16_t)std::clamp<int32_t>(xy.height, 0i16, UINT16_MAX);
		//}

		void Item::get_sliced_regions(
			const Rect& guide,
			const Item::Transformation xy_transform,
			Container<Container<Vertex>>& result
		) const
		{
			if (!is_rectangle()) return;
			using namespace Clipper2Lib;

			PathsD clip, solution;

			PathD subject;
			subject.reserve(vertices.size());
			for (const Vertex& vertex : vertices)
			{
				subject.emplace_back(
					vertex.xy.x + xy_transform.translation.x,
					vertex.xy.y + xy_transform.translation.y
				);
			}

			const double min = -0xFFFFFF;
			const double max = 0xFFFFFF;
			clip.push_back({ PointD((const double)guide.left, min), PointD((const double)guide.left, max) });
			clip.push_back({ PointD((const double)guide.right, min), PointD((const double)guide.right, max) });
			clip.push_back({ PointD(min, (const double)guide.bottom), PointD(max, (const double)guide.bottom) });
			clip.push_back({ PointD(min, (const double)guide.top), PointD(max, (const double)guide.top) });

			solution = Intersect(PathsD({ subject }), clip, FillRule::NonZero);

			for (size_t i = 0; solution.size() > i; i++)
			{
				auto& path = solution[i];
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
			Container<Container<cv::Point>> contours;
			findContours(image, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

			for (std::vector<cv::Point>& points : contours) {
				std::move(points.begin(), points.end(), std::back_inserter(result));
			}
		}

		void Item::normalize_mask(cv::Mat& mask)
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
		}
	}
}