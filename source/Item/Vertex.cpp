#include "AtlasGenerator/Item/Vertex.h"

namespace sc
{
	namespace AtlasGenerator
	{
		Vertex::Vertex() {
			xy = Point<uint16_t>(0, 0);
			uv = Point<uint16_t>(0, 0);
		};

		Vertex::Vertex(uint16_t x, uint16_t y, uint16_t u, uint16_t v) {
			xy = Point<uint16_t>(x, y);
			uv = Point<uint16_t>(u, v);
		};
	}
}