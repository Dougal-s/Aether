#include <cstddef>
#include <random>

#include <benchmark/benchmark.h>

#include "DSP/diffuser.hpp"

namespace {
	std::mt19937 rng{std::random_device{}()};
}

static void diffuser_push(benchmark::State& state) {
	static constexpr double samplerate = 48000;
	AllpassDiffuser<double> diffuser(samplerate, rng);
	diffuser.set_delay(AllpassDiffuser<double>::delay_bounds.first*samplerate);
	diffuser.set_mod_rate(std::uniform_real_distribution<double>(0.f, 5.f)(rng));
	diffuser.set_mod_depth(AllpassDiffuser<double>::mod_bounds.second*samplerate);
	AllpassDiffuser<double>::PushInfo info = {
		.stages = AllpassDiffuser<double>::max_stages,
		.feedback = 0.5,
		.interpolate = false
	};
	for (auto _ : state)
		benchmark::DoNotOptimize(diffuser.push(0.5, info));
}

static void diffuser_push_interpolate(benchmark::State& state) {
	static constexpr double samplerate = 48000;
	AllpassDiffuser<double> diffuser(samplerate, rng);
	diffuser.set_delay(AllpassDiffuser<double>::delay_bounds.first*samplerate);
	diffuser.set_mod_rate(std::uniform_real_distribution<double>(0.f, 5.f)(rng));
	diffuser.set_mod_depth(AllpassDiffuser<double>::mod_bounds.second*samplerate);
	AllpassDiffuser<double>::PushInfo info = {
		.stages = AllpassDiffuser<double>::max_stages,
		.feedback = 0.5,
		.interpolate = true
	};
	for (auto _ : state)
		benchmark::DoNotOptimize(diffuser.push(0.5, info));
}

BENCHMARK(diffuser_push)->Unit(benchmark::kNanosecond);
BENCHMARK(diffuser_push_interpolate)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
