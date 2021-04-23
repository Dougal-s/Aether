#include <random>

#include <gtest/gtest.h>

#include "common/constants.hpp"
#include "DSP/utils/lfo.hpp"

namespace {
	std::mt19937 rng{std::random_device{}()};
}

// checks that
TEST(lfo, bounds_check) {
	LFO lfo(0.f, 0.25f);
	for (size_t i = 0; i < 100'000'000; ++i)
		lfo.next();
	ASSERT_LE(lfo.depth(), 1.f);
	ASSERT_GE(lfo.depth(), -1.f);
}

TEST(lfo, bounds_check_randomized) {
	std::uniform_real_distribution<float> dist{0.f, 1.f};
	LFO lfo(dist(rng), dist(rng));
	for (size_t i = 0; i < 100'000'000; ++i) {
		ASSERT_LE(lfo.depth(), 1.f);
		ASSERT_GE(lfo.depth(), -1.f);
		lfo.next();
	}
}

TEST(lfo, value_check_randomized) {
	using namespace constants;

	std::uniform_real_distribution<float> dist{0.f, 1.f};
	float phase = dist(rng);
	float rate = dist(rng);
	LFO lfo(phase, rate);

	static constexpr size_t steps = 1'000;
	for (size_t i = 0; i < steps; ++i)
		lfo.next();

	ASSERT_NEAR(
		lfo.depth(),
		std::sin(2*pi*(phase + std::fmod(rate*steps, 1.f))),
		0.001f
	);
}
