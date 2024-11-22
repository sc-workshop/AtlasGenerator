#pragma once

#include "Item.h"

#include <core/math/point.h>
#include <core/math/line.h>
#include <core/math/triangle.h>

#include <optional>
#include <cmath>

namespace wk
{
	namespace AtlasGenerator
	{
		template<typename T1, typename T2>
		float dist(const Point_t<T1>& p1, const Point_t<T2>& p2) {
			return std::sqrt(std::pow(p1.x - p2.x, 2) + std::pow(p1.y - p2.y, 2));
		}

		// https://flassari.is/2008/11/line-line-intersection-in-cplusplus
		std::optional<PointF> line_intersect(const LineF l1, const LineF l2)
		{
			float x1 = l1.start.x, x2 = l1.end.x, x3 = l2.start.x, x4 = l2.end.x;
			float y1 = l1.start.y, y2 = l1.end.y, y3 = l2.start.y, y4 = l2.end.y;

			float determinant = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

			if (determinant == 0)
			{
				return std::nullopt;
			};

			// Get the x and y
			float pre = (x1 * y2 - y1 * x2), post = (x3 * y4 - y3 * x4);
			float x = (pre * (x3 - x4) - (x1 - x2) * post) / determinant;
			float y = (pre * (y3 - y4) - (y1 - y2) * post) / determinant;

			// Check if the x and y coordinates are within both lines
			if (x < std::min(x1, x2) || x > std::max(x1, x2) ||
				x < std::min(x3, x4) || x > std::max(x3, x4)) 
			{
				return std::nullopt;
			};

			if (y < std::min(y1, y2) || y > std::max(y1, y2) ||
				y < std::min(y3, y4) || y > std::max(y3, y4))
			{
				return std::nullopt;
			};

			return PointF(x, y);
		}

		bool line_intersect(const Container<Point>& polygon, const LineF line, size_t& p1_idx_res, size_t& p2_idx_res, PointF& intersect)
		{
			size_t length = polygon.size();
			for (size_t i = 0; length > i; i++)
			{
				size_t p1_idx = i;
				size_t p2_idx = (i + 1) % length;

				const Point& p1 = polygon[p1_idx];
				const Point& p2 = polygon[p2_idx];

				LineF candidate(
					LineF::ValueT((float)p1.x, (float)p1.y),
					LineF::ValueT((float)p2.x, (float)p2.y)
				);

				auto result = line_intersect(candidate, line);
				if (result.has_value())
				{
					intersect = result.value();
					p1_idx_res = p1_idx;
					p2_idx_res = p2_idx;
					return true;
				}
			}

			return false;
		}

		template<typename T>
		float line_angle(const Line_t<T>& line)
		{
			return std::atan2(line.end.y - line.start.y, line.end.x - line.start.x);
		}

		template<typename T>
		Triangle_t<T> build_triangle(const Line_t<T>& bisector, float angle, const T& length)
		{
			T half_length = (T)((float)length / 2);

			const Point_t<T>& midpoint = bisector.end;

			T x1 = midpoint.x + half_length * cos(angle);
			T y1 = midpoint.y + half_length * sin(angle);

			T x2 = midpoint.x - half_length * cos(angle);
			T y2 = midpoint.y - half_length * sin(angle);

			return Triangle_t<T>(
				bisector.start,
				Point_t<T>(x1, y1),
				Point_t<T>(x2, y2)
			);
		}
	}
}