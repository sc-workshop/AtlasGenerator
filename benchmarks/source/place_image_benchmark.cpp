#include <benchmark/benchmark.h>

#include <cstdint>

#include "atlas_generator/Config.h"
#include "atlas_generator/Generator.h"
#include "atlas_generator/Item/Item.h"

#include "core/image/raw_image.h"
#include "core/math/color_rgba.h"

using namespace wk;
using namespace wk::AtlasGenerator;

static void BM_PlaceImage(benchmark::State& state) {
	const uint16_t size = static_cast<uint16_t>(state.range(0));
	const Config config(1024, 1024, 1.0f, 2);

	RawImage input(size, size, Image::PixelDepth::RGBA8);
	for (uint16_t y = 0; size > y; ++y) {
		for (uint16_t x = 0; size > x; ++x) {
			input.at<ColorRGBA>(x, y) = ColorRGBA{ 10, 20, 30, 255 };
		}
	}

	for (auto _ : state) {
		RawImage atlas(1024, 1024, Image::PixelDepth::RGBA8);
		Generator::place_image_to(config, input, 8, 8, Item::NoRotation, atlas);
		benchmark::DoNotOptimize(atlas.data());
		benchmark::ClobberMemory();
	}
}

BENCHMARK(BM_PlaceImage)
	->Arg(32)
	->Arg(128)
	->Arg(256)
	->Unit(benchmark::kMicrosecond);
