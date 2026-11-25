#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "atlas_generator/Item/Item.h"

namespace atlas_bench
{
	struct LoadedSet
	{
		std::vector<wk::AtlasGenerator::Item> items;
		size_t skipped = 0;
	};

	// Loads a test directory the same way the CLI does (PNG + _la + _guide),
	// but builds items from RawImage so benchmarks do not need OpenCV codecs.
	LoadedSet load_items(const std::filesystem::path& directory);

	size_t count_png(const std::filesystem::path& directory);
}
