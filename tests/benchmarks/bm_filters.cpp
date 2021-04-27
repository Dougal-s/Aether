#include <benchmark/benchmark.h>

#include "DSP/filters.hpp"

static void bm_lowpass6dB(benchmark::State& state) {
	Lowpass6dB<float> lp(48000, 500);
	for (auto _ : state)
		benchmark::DoNotOptimize(lp.push(0.5f));
}

static void bm_highpass6dB(benchmark::State& state) {
	Highpass6dB<float> hp(48000, 500);
	for (auto _ : state)
		benchmark::DoNotOptimize(hp.push(0.5f));
}

static void bm_lowshelf(benchmark::State& state) {
	Lowshelf<float> ls(48000);
	ls.set_cutoff(100.f);
	ls.set_gain(2.f);
	for (auto _ : state)
		benchmark::DoNotOptimize(ls.push(0.5f));
}

static void bm_highshelf(benchmark::State& state) {
	Highshelf<float> hs(48000);
	hs.set_cutoff(100.f);
	hs.set_gain(2.f);
	for (auto _ : state)
		benchmark::DoNotOptimize(hs.push(0.5f));
}

BENCHMARK(bm_lowpass6dB)->Unit(benchmark::kNanosecond);
BENCHMARK(bm_highpass6dB)->Unit(benchmark::kNanosecond);
BENCHMARK(bm_lowshelf)->Unit(benchmark::kNanosecond);
BENCHMARK(bm_highshelf)->Unit(benchmark::kNanosecond);


BENCHMARK_MAIN();
