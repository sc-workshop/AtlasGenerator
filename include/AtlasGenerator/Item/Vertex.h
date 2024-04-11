#pragma once

// SC CORE
#include <math/point.h>

#include <stdint.h>

namespace sc
{
	namespace AtlasGenerator
	{
		class Vertex {
		public:
			Vertex();
			Vertex(uint16_t x, uint16_t y, uint16_t u, uint16_t v);

		public:
			Point<uint16_t> uv;
			Point<uint16_t> xy;
		};
	}
}