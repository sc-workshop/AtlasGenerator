#pragma once

#include <functional>

namespace wk::AtlasGenerator::packing {
    struct PackingConfig {
        double accuracy = 0.6;
        bool parallel = true;
        bool sort_by_max_dimension = true;
        std::function<void()> progress;
    };
}
