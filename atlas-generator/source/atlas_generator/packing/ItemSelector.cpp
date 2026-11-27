#include "ItemSelector.h"

#include "core/algorithm/sort.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace wk::AtlasGenerator::packing {
    size_t pack_items(std::vector<PackingItem>& items,
                      const Box& bin,
                      int min_object_distance,
                      const PackingConfig& config) {
        const Coord inflation = static_cast<Coord>(std::ceil(static_cast<double>(min_object_distance) / 2.0));

        if (inflation > 0) {
            for (PackingItem& item : items)
                item.inflate(inflation);
            for (PackingItem& item : items)
                item.prepare();
        }

        std::vector<NfpPacker> placers;
        placers.reserve(items.size());

        std::vector<std::reference_wrapper<PackingItem>> store;
        store.reserve(items.size());
        for (PackingItem& item : items)
            store.emplace_back(item);

        std::sort(store.begin(),
                  store.end(),
                  [&config](const std::reference_wrapper<PackingItem>& a,
                            const std::reference_wrapper<PackingItem>& b) {
                      if (config.sort_by_max_dimension) {
                          const Box ab = a.get().bounding_box();
                          const Box bb = b.get().bounding_box();
                          const Coord amax = std::max(ab.width(), ab.height());
                          const Coord bmax = std::max(bb.width(), bb.height());
                          if (amax != bmax)
                              return amax > bmax;
                      }
                      return a.get().area() > b.get().area();
                  });

        for (std::reference_wrapper<PackingItem>& item_ref : store) {
            PackingItem& item = item_ref.get();
            bool packed = false;
            size_t bin_index = 0;

            while (!packed) {
                if (bin_index < placers.size()) {
                    packed = placers[bin_index].try_pack(item);
                    if (packed) {
                        item.bin_id(static_cast<int>(bin_index));
                        placers[bin_index].accept(item);
                    } else {
                        ++bin_index;
                    }
                    continue;
                }

                placers.emplace_back(bin, config);
                packed = placers.back().try_pack(item);
                if (packed) {
                    item.bin_id(static_cast<int>(placers.size() - 1));
                    placers.back().accept(item);
                } else {
                    ++bin_index;
                }
            }

            if (config.progress)
                config.progress();
        }

        if (min_object_distance > 0) {
            for (PackingItem& item : items)
                item.inflate(-inflation);
            for (PackingItem& item : items)
                item.prepare();
        }

        return placers.size();
    }
}
