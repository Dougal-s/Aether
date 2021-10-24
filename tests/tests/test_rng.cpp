#include <array>
#include <random>

#include <gtest/gtest.h>

#include "DSP/utils/random.hpp"

// very crude test just to make sure that the rng
// hasn't gone terribly wrong

// Checks that each sequence of 8bits occurs roughly the same number of times
TEST(rng, sequence) {
	Random::Xorshift64s rng{std::random_device{}()};

	constexpr long gen_num = 1'000'000;
	std::array<long, 1<<8> counts = {};
	for (long i = 0; i < gen_num; ++i) {
		uint32_t num = rng();
		++counts[(num & 0xff) >> 0];
		++counts[(num & 0xff00) >> 8];
		++counts[(num & 0xff0000) >> 16];
		++counts[(num & 0xff000000) >> 24];
	}

	long expected = 4*gen_num/counts.size();
	for (auto count : counts)
		EXPECT_LE(std::abs(count - expected), 5*expected/100);
}
