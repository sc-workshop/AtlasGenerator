#include "AtlasGenerator/Item/Vertex.h"

namespace sc
{
	namespace AtlasGenerator
	{
		Vertex::Vertex() {
			xy = Point<int32_t>(0, 0);
			uv = Point<uint16_t>(0, 0);
		};

		Vertex::Vertex(int32_t x, int32_t y, uint16_t u, uint16_t v) {
			xy = Point<int32_t>(x, y);
			uv = Point<uint16_t>(u, v);
		};
	}
}