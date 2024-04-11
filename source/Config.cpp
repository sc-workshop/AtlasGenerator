#include "AtlasGenerator/Config.h"

#define MinTextureDimension 512
#define MaxTextureDimension 4096

#define MinExtrude 1
#define MaxExtrude 6

#define MinScaleFactor 1
#define MaxScaleFactor 4

namespace sc
{
	namespace AtlasGenerator
	{
		Config::Config(
			Config::TextureType type,
			uint16_t width, uint16_t height,
			uint8_t scale, uint8_t extrude)
			: m_texture_type(type),
			m_max_width(std::clamp<uint16_t>(width, MinTextureDimension, MaxTextureDimension)),
			m_max_height(std::clamp<uint16_t>(height, MinTextureDimension, MaxTextureDimension)),
			m_scale_factor(std::clamp<uint8_t>(scale, MinScaleFactor, MaxScaleFactor)),
			m_extrude(std::clamp<uint8_t>(extrude, MinExtrude, MaxExtrude))
		{}
	}
}