#include <cstddef>
#include <random>

#include <benchmark/benchmark.h>

#include "DSP/diffuser.hpp"

namespace {
	std::mt19937 rng{std::random_device{}()};
}

static void diffuser_push(benchmark::State& state) {
	static constexpr float samplerate = 48000;
	AllpassDiffuser diffuser(samplerate, rng);
	diffuser.set_delay(AllpassDiffuser::delay_bounds.first*samplerate);
	diffuser.set_mod_rate(std::uniform_real_distribution<float>(0.f, 5.f)(rng));
	diffuser.set_mod_depth(AllpassDiffuser::mod_bounds.second*samplerate);
	AllpassDiffuser::PushInfo info = {
		.stages = AllpassDiffuser::max_stages,
		.feedback = 0.5f,
		.interpolate = false
	};
	for (auto _ : state)
		benchmark::DoNotOptimize(diffuser.push(0.5f, info));
}

static void diffuser_push_interpolate(benchmark::State& state) {
	static constexpr float samplerate = 48000;
	AllpassDiffuser diffuser(samplerate, rng);
	diffuser.set_delay(AllpassDiffuser::delay_bounds.first*samplerate);
	diffuser.set_mod_rate(std::uniform_real_distribution<float>(0.f, 5.f)(rng));
	diffuser.set_mod_depth(AllpassDiffuser::mod_bounds.second*samplerate);
	AllpassDiffuser::PushInfo info = {
		.stages = AllpassDiffuser::max_stages,
		.feedback = 0.5f,
		.interpolate = true
	};
	for (auto _ : state)
		benchmark::DoNotOptimize(diffuser.push(0.5f, info));
}

BENCHMARK(diffuser_push)->Unit(benchmark::kNanosecond);
BENCHMARK(diffuser_push_interpolate)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
