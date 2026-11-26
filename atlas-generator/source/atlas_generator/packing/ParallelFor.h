#pragma once

#include <cstddef>
#include <functional>

namespace wk::AtlasGenerator::packing {
    // Runs body(index) for every index in [0, count) on the custom std thread
    // pool. Callers write to distinct result slots so reduction order stays
    // deterministic. ATLAS_GENERATOR_THREADS overrides the detected thread
    // count. A count of one runs inline to avoid waking workers.
    void parallel_for_indexed(size_t count, const std::function<void(size_t)>& body);
}
