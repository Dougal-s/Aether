#include <cstddef>

#include <benchmark/benchmark.h>

#include "DSP/delay.hpp"

static void delay_push(benchmark::State& state) {
	Delay delay(48000);
	for (auto _ : state)
		benchmark::DoNotOptimize(delay.push(0.5f, 100));
}

static void modulated_delay_push(benchmark::State& state) {
	ModulatedDelay<float> delay(48000, 0);
	delay.set_delay(10);
	delay.set_mod_depth(5);
	delay.set_mod_rate(0.1f);
	for (auto _ : state)
		benchmark::DoNotOptimize(delay.push(0.5f));
}

static void multitapdelay_push(benchmark::State& state) {
	MultitapDelay delay(48000);
	for (auto _ : state)
		benchmark::DoNotOptimize(
			delay.push(0.5f, MultitapDelay::max_taps, 0.25f)
		);
}

BENCHMARK(delay_push)->Unit(benchmark::kNanosecond);
BENCHMARK(multitapdelay_push)->Unit(benchmark::kNanosecond);
BENCHMARK(modulated_delay_push)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
