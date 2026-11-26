#pragma once

#include "NfpPacker.h"
#include "PackingConfig.h"
#include "PackingTypes.h"

#include <cstddef>

namespace wk::AtlasGenerator::packing {
    size_t pack_items(std::vector<PackingItem>& items,
                      const Box& bin,
                      int min_object_distance,
                      const PackingConfig& config);

}
