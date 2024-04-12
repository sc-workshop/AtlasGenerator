#pragma once

// SC CORE
#include <math/point.h>
#include <math/rect.h>

#include <stdint.h>

namespace sc
{
	namespace AtlasGenerator
	{
		class Vertex {
		public:
			Vertex();
			Vertex(int32_t x, int32_t y, uint16_t u, uint16_t v);

		public:
			Point<uint16_t> uv;
			Point<int32_t> xy;
		};
	}
}