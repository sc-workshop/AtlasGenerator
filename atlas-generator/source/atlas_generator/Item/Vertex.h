#pragma once

// SC CORE
#include "core/math/point.h"
#include "core/math/rect.h"

#include <stdint.h>

namespace sc
{
	namespace AtlasGenerator
	{
		using Rect = Rect_t<int32_t>;
		using Point = Point_t<int32_t>;

		using RectUV = Rect_t<uint16_t>;
		using PointUV = Point_t<uint16_t>;

		class Vertex {
		public:
			Vertex();
			Vertex(int32_t x, int32_t y, uint16_t u, uint16_t v);

		public:
			PointUV uv;
			Point xy;
		};
	}
}