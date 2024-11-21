#include "Config.h"
#include "limits.h"

namespace sc
{
	namespace AtlasGenerator
	{
		Config::Config(
			Config::TextureType type,
			uint16_t width, uint16_t height,
			float scale, uint8_t extrude)
			: m_texture_type(type),
			m_max_width(std::clamp<uint16_t>(width, MinTextureDimension, MaxTextureDimension)),
			m_max_height(std::clamp<uint16_t>(height, MinTextureDimension, MaxTextureDimension)),
			m_scale_factor(std::clamp<uint8_t>(scale, MinScaleFactor, MaxScaleFactor)),
			m_extrude(std::clamp<uint8_t>(extrude, MinExtrude, MaxExtrude))
		{}
	}
}