#include "atlas_generator/Item/Item.h"

#include "core/math/triangle.h"
#include "core/geometry/intersect.hpp"
#include "core/geometry/convex.hpp"
#include "core/stb/stb.h"
#include "core/io/file_stream.h"

#include <cmath>
#include <clipper2/clipper.h>

namespace wk
{
	namespace AtlasGenerator
	{
		Item::Transformation::Transformation(double rotation, Point translation) : rotation(rotation), translation(translation)
		{
		}

		Item::Item(const RawImage& image, bool sliced) : m_sliced(sliced), m_image(wk::CreateRef<RawImage>(image))
		{
		}

		Item::Item(const ColorRGBA& color) : m_image(wk::CreateRef<RawImage>(color)), m_colorfill(true)
		{
		}

#ifdef ATLAS_GENERATOR_WITH_IMAGE_CODECS
		Item::Item(std::filesystem::path path, bool sliced) :
			m_sliced(sliced)
		{
			InputFileStream file(path);
			stb::load_image(file, m_image);
		}
#endif

		Item::Status Item::status() const { return m_status; }
		uint16_t Item::width() const { return (uint16_t)m_image->width(); };
		uint16_t Item::height() const { return (uint16_t)m_image->height(); };

		bool Item::is_rectangle() const
		{
			if (is_sliced()) return true;

			return 100 > width() + height();
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
			if (!verify_vertices()) return false;

			m_status = Status::Valid;
			mark_as_preprocessed();
			return true;
		}

		bool Item::mark_as_preprocessed()
		{
			m_preprocessed = true;
			return true;
		}

		void Item::generate_image_polygon(const Config& config)
		{
			using namespace wk::Geometry;

			float scale_factor = is_sliced() ? 1.0f : config.scale();
			Image::Size full_size = m_image->size();
			Image::Size current_size = full_size;
			PointF crop_offset(0.0f, 0.0f);

			auto fallback_rectangle = [this, &crop_offset, &full_size, &current_size]
				{
					vertices.resize(4);

					vertices[3].uv = { 0,												0 };
					vertices[2].uv = { 0,												(uint16_t)current_size.y };
					vertices[1].uv = { (uint16_t)current_size.x,					(uint16_t)current_size.y };
					vertices[0].uv = { (uint16_t)current_size.x,					0 };

					vertices[3].xy = { (uint16_t)crop_offset.x,								(uint16_t)crop_offset.y };
					vertices[2].xy = { (uint16_t)crop_offset.x,								(uint16_t)(crop_offset.y + current_size.y) };
					vertices[1].xy = { (uint16_t)(crop_offset.x + current_size.x),		(uint16_t)(crop_offset.y + current_size.y) };
					vertices[0].xy = { (uint16_t)(crop_offset.x + current_size.x),		(uint16_t)crop_offset.y };

					m_status = Status::Valid;
				};
			image_preprocess(config);

			if (1 >= width() || 1 >= height())
			{
				fallback_rectangle();
				return;
			}

			RawImageRef alpha_mask;
			switch (m_image->channels())
			{
			case 4:
				m_image->extract_channel(alpha_mask, 3);
				break;
			case 2:
				m_image->extract_channel(alpha_mask, 1);
				break;
			default:
				fallback_rectangle();
				return;
			}

			normalize_mask(alpha_mask, config);

			Image::Bound crop_bound = alpha_mask->bound();
			if (crop_bound.width <= 0) crop_bound.width = 1;
			if (crop_bound.height <= 0) crop_bound.height = 1;

			// Image cropping by alpha
			m_image = m_image->crop(crop_bound);
			alpha_mask = alpha_mask->crop(crop_bound);

			current_size = alpha_mask->size();
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

			Container<Point> polygon;
			{
				Container<Point> contour;
				get_image_contour(alpha_mask, contour);

				// Getting convex hull as base polygon for calculations
				polygon = Hull::quick_hull(contour);
			}

			Container<Triangle> triangles;
			triangles.reserve(4);

			Point centroid = {
				static_cast<int>(abs(static_cast<float>(current_size.x) / 2)),
				static_cast<int>(abs(static_cast<float>(current_size.y) / 2))
			};

			float distance_threshold = (current_size.x + current_size.y) * 0.03f;

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
						cutoff_bisector, angle, (current_size.x + current_size.y) * 2
					);

					triangles.push_back(cutoff);
				};

			Rect bounding_box = { 0, 0, current_size.x, current_size.y };

			calculate_triangle(Point(0, 0));
			calculate_triangle(Point(current_size.x, 0));
			calculate_triangle(Point(current_size.x, current_size.y));
			calculate_triangle(Point(0, current_size.y));

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
				subject.emplace_back(current_size.x, 0);
				subject.emplace_back(current_size.x, current_size.y);
				subject.emplace_back(0, current_size.y);

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

				PathD path;
				for (auto& candidate : solution)
				{
					if (candidate.size() > 3)
					{
						path = candidate;
						break;
					}
				}

				vertices.reserve(path.size());
				for (auto& point : path)
				{
					int32_t x = (int32_t)std::ceil((point.x + crop_bound.x) * scale_factor);
					int32_t y = (int32_t)std::ceil((point.y + crop_bound.y) * scale_factor);
					uint16_t u = (uint16_t)std::ceil(point.x);
					uint16_t v = (uint16_t)std::ceil(point.y);

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

				uv_size.x = (uint16_t)std::abs(uv_bound.left - uv_bound.right);
				uv_size.y = (uint16_t)std::abs(uv_bound.top - uv_bound.bottom);
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
						(float)(path_vertex.x - offset.x) / size.x,
						(float)(path_vertex.y - offset.y) / size.y
					);

					vertex.uv.x = (uint16_t)(uv_coord.x * uv_size.x);
					vertex.uv.y = (uint16_t)(uv_coord.y * uv_size.y);
				}
			}
		}

		bool Item::operator ==(const Item& other) const
		{
			if (std::addressof(image()) == std::addressof(other.image())) return true;

			if (!m_hash)
			{
				m_hash = m_image->hash();
			}

			if (m_hash != other.m_hash) return false;

			return true;
		}

		void Item::image_preprocess(const Config& config)
		{
			if (m_preprocessed) return;
			if (config.scale() != 1.0f && !is_sliced())
			{
				RawImageRef resized = CreateRef<RawImage>(
					(uint16_t)ceil(m_image->width() * config.scale()),
					(uint16_t)ceil(m_image->height() * config.scale()),
					m_image->depth(),
					m_image->colorspace()
				);

				m_image->copy(*resized);
				m_image = resized;
			}

			int channels = m_image->channels();

			if (channels == 2 || channels == 4) {
				alpha_preprocess();
			}

			m_preprocessed = true;
		}
		void Item::alpha_preprocess()
		{
			int channels = m_image->channels();

			for (uint16_t h = 0; height() > h; h++) {
				for (uint16_t w = 0; width() > w; w++) {
					switch (channels)
					{
					case 4:
					{
						ColorRGBA& pixel = m_image->at<ColorRGBA>(w, h);
						float alpha = (float)pixel.a / 255.f;

						pixel.r = (uint8_t)(pixel.r * alpha);
						pixel.g = (uint8_t)(pixel.g * alpha);
						pixel.b = (uint8_t)(pixel.b * alpha);

					}
					break;
					case 2:
					{
						ColorLA& pixel = m_image->at<ColorLA>(w, h);
						float alpha = (float)pixel.a / 255.f;

						pixel.l = (uint8_t)(pixel.l * alpha);
					}
					break;
					}
				}
			}
		}

		void Item::get_image_contour(RawImageRef& image, Container<Point>& result)
		{
			for (Image::SizeT h = 0; image->height() > h; h++) {
				for (Image::SizeT w = 0; image->width() > w; w++) {
					uint8_t pixel = image->at<uint8_t>(w, h);
					bool valid = false;

					// Iterate over black pixels only
					if (pixel > 1)
					{
						if (h == 0 || w == 0 || h == image->height() - 1 || w == image->width() - 1)
						{
							result.emplace_back(w, h);
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
							if (x >= image->width() || y >= image->height()) continue;

							uint8_t neighbor = image->at<uint8_t>(
								(Image::SizeT)x, 
								(Image::SizeT)y
							);

							has_positive = has_positive || neighbor > 0;
							has_negative = has_negative || neighbor == 0;
						}
					}

					valid = has_negative && has_positive;
					if (valid)
					{
						result.emplace_back(w, h);
					}
				}
			}
		}

		void Item::normalize_mask(RawImageRef& mask, const Config& config)
		{
			// Pixel Normalize
			for (uint16_t h = 0; mask->height() > h; h++) {
				for (uint16_t w = 0; mask->width() > w; w++) {
					uint8_t& pixel = mask->at<uint8_t>(w, h);

					if (pixel > config.alpha_threshold()) {
						pixel = 255;
					}
					else
					{
						pixel = 0;
					};
				}
			}

			//const int size = 2;
			//cv::Mat element = cv::getStructuringElement(cv::MORPH_CROSS,
			//	cv::Size(2 * size + 1, 2 * size + 1),
			//	cv::Point(size, size));
			//cv::dilate(mask, mask, element);
		}

		bool Item::verify_vertices()
		{
			std::vector<PointUV> points;
			points.resize(vertices.size());
			for (size_t i = 0; vertices.size() > i; i++)
			{
				points[i] = vertices[i].uv;
			}

			if (Geometry::get_polygon_type(points) != Geometry::PolygonType::Convex)
			{
				m_status = Status::InvalidPolygon;
				return false;
			}

			return true;
		}
	}
}