#include "Config.h"
#include "limits.h"

namespace wk
{
	namespace AtlasGenerator
	{
		Config::Config(
			uint16_t width, uint16_t height,
			float scale, uint8_t extrude, bool parallel, uint8_t alpha_threshold)
			:
			m_max_width(std::clamp<uint16_t>(width, MinTextureDimension, MaxTextureDimension)),
			m_max_height(std::clamp<uint16_t>(height, MinTextureDimension, MaxTextureDimension)),
			m_scale_factor(std::clamp<float>(scale, MinScaleFactor, MaxScaleFactor)),
			m_extrude(std::clamp<uint8_t>(extrude, MinExtrude, MaxExtrude)),
			m_parallel(parallel),
			m_alpha_threshold(alpha_threshold)
		{}
	}
}