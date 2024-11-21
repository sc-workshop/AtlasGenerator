#include "Vertex.h"

namespace sc
{
	namespace AtlasGenerator
	{
		Vertex::Vertex() {
			xy = Point(0, 0);
			uv = PointUV(0, 0);
		};

		Vertex::Vertex(int32_t x, int32_t y, uint16_t u, uint16_t v) {
			xy = Point(x, y);
			uv = PointUV(u, v);
		};
	}
}