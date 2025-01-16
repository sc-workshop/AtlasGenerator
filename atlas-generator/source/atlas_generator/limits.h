#pragma once

#include <stdint.h>

namespace wk
{
	namespace AtlasGenerator
	{
		constexpr size_t MinTextureDimension = 512;
		constexpr size_t MaxTextureDimension = 8096;

		constexpr uint8_t MinExtrude = 0;
		constexpr uint8_t MaxExtrude = 10;

		constexpr float MinScaleFactor = 0.25f;
		constexpr float MaxScaleFactor = 10.0f;
	}
}
