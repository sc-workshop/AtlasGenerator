#include <benchmark/benchmark.h>

#include <cstddef>
#include <string>

#include "atlas_generator/Config.h"
#include "atlas_generator/Generator.h"

#include "dataset.h"

using namespace wk::AtlasGenerator;

namespace
{
	Config make_config() {
		return Config(4096, 4096, 1.0f, 2);
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

// Full load path: stb decode, _la conversion, guide detection.
static void BM_LoadImages(benchmark::State& state, const std::string& dataset) {
	for (auto _ : state) {
		atlas_bench::LoadedSet set = atlas_bench::load_items(dataset);
		benchmark::DoNotOptimize(set.items.size());
		state.counters["items"] = static_cast<double>(set.items.size());
		state.counters["skipped"] = static_cast<double>(set.skipped);
	}
}

// One generation per run; use --benchmark_repetitions=N for statistics.
// Small currently takes minutes, so do not hard-code repetitions here.
BENCHMARK_CAPTURE(BM_Generate, small, "test/small")->Unit(benchmark::kMillisecond)->Iterations(1);
BENCHMARK_CAPTURE(BM_Generate, medium, "test/medium")->Unit(benchmark::kMillisecond)->Iterations(1);
BENCHMARK_CAPTURE(BM_Generate, large, "test/large")->Unit(benchmark::kMillisecond)->Iterations(1);

BENCHMARK_CAPTURE(BM_LoadImages, small, "test/small")->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(BM_LoadImages, medium, "test/medium")->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(BM_LoadImages, large, "test/large")->Unit(benchmark::kMillisecond);
