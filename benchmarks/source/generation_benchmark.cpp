#include "atlas_generator/Config.h"
#include "atlas_generator/Generator.h"
#include "atlas_generator/Item/Item.h"
#include "core/image/raw_image.h"
#include "dataset.h"

#include <benchmark/benchmark.h>
#include <cstddef>
#include <string>
#include <vector>

using namespace wk;
using namespace wk::AtlasGenerator;

namespace {
    Config make_config() {
        return Config(4096, 4096, 1.0f, 2);
    }

    // Fresh image copies are needed because generate_image_polygon() mutates the
    // item image through preprocessing, alpha thresholding, and cropping.
    struct PolygonSource {
        explicit PolygonSource(const RawImage& image, bool sliced) :
            image(image),
            sliced(sliced) {}

        RawImage image;
        bool sliced;
    };

    struct PolygonDataset {
        std::vector<PolygonSource> sources;
        size_t skipped = 0;
    };

    PolygonDataset load_polygon_dataset(const std::string& dataset) {
        atlas_bench::LoadedSet set = atlas_bench::load_items(dataset);

        PolygonDataset result;
        result.skipped = set.skipped;
        result.sources.reserve(set.items.size());
        for (const Item& item : set.items) {
            result.sources.emplace_back(item.image(), item.is_sliced());
        }

        return result;
    }
}

// Timing covers Generator::generate only. Loading from disk is paused so the
// number reflects the generation path and can be compared across datasets.
static void BM_Generate(benchmark::State& state, const std::string& dataset) {
    const Config config = make_config();

    for (auto _ : state) {
        state.PauseTiming();
        atlas_bench::LoadedSet set = atlas_bench::load_items(dataset);
        state.ResumeTiming();

        Generator generator(config);
        const size_t bin_count = generator.generate(set.items);
        benchmark::DoNotOptimize(bin_count);

        state.counters["items"] = static_cast<double>(set.items.size());
        state.counters["skipped"] = static_cast<double>(set.skipped);
    }
}

// Times only Item::generate_image_polygon(). Dataset loading and fresh Item
// construction happen with timing paused so repeated runs start from identical
// unprocessed images.
static void BM_GenerateImagePolygon(benchmark::State& state, const std::string& dataset) {
    const Config config = make_config();
    const PolygonDataset data = load_polygon_dataset(dataset);
    size_t vertices = 0;

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<Item> items;
        items.reserve(data.sources.size());
        for (const PolygonSource& source : data.sources) {
            items.emplace_back(source.image, source.sliced);
        }
        state.ResumeTiming();

        for (Item& item : items) {
            item.generate_image_polygon(config);
        }

        state.PauseTiming();
        vertices = 0;
        for (const Item& item : items) {
            vertices += item.vertices.size();
        }
        benchmark::DoNotOptimize(vertices);
        benchmark::ClobberMemory();
        state.ResumeTiming();

        state.counters["items"] = static_cast<double>(data.sources.size());
        state.counters["skipped"] = static_cast<double>(data.skipped);
        state.counters["vertices"] = static_cast<double>(vertices);
    }
}

// Full load path: stb decode, _la conversion, guide detection.
static void BM_LoadImages(benchmark::State& state, const std::string& dataset) {
    for (auto _ : state) {
        atlas_bench::LoadedSet set = atlas_bench::load_items(dataset);
        benchmark::DoNotOptimize(set.items.size());
        state.counters["items"] = static_cast<double>(set.items.size());
        state.counters["skipped"] = static_cast<double>(set.skipped);
    }
}

// One generation per run. Use --benchmark_repetitions=N for statistics.
// Small currently takes minutes, so do not hard-code repetitions here.
BENCHMARK_CAPTURE(BM_Generate, small, "test/small")->Unit(benchmark::kMillisecond)->Iterations(1);
BENCHMARK_CAPTURE(BM_Generate, medium, "test/medium")->Unit(benchmark::kMillisecond)->Iterations(1);
BENCHMARK_CAPTURE(BM_Generate, large, "test/large")->Unit(benchmark::kMillisecond)->Iterations(1);

BENCHMARK_CAPTURE(BM_GenerateImagePolygon, small, "test/small")->Unit(benchmark::kMillisecond)->Iterations(1);
BENCHMARK_CAPTURE(BM_GenerateImagePolygon, medium, "test/medium")->Unit(benchmark::kMillisecond)->Iterations(1);
BENCHMARK_CAPTURE(BM_GenerateImagePolygon, large, "test/large")->Unit(benchmark::kMillisecond)->Iterations(1);

BENCHMARK_CAPTURE(BM_LoadImages, small, "test/small")->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(BM_LoadImages, medium, "test/medium")->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(BM_LoadImages, large, "test/large")->Unit(benchmark::kMillisecond);
