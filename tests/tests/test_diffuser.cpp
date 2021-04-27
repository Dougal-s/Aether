#include <random>

#include <gtest/gtest.h>

#include "DSP/diffuser.hpp"


// checks for crashes or failed asserts
TEST(diffuser, error_check) {
	std::mt19937 rng{std::random_device{}()};

	static constexpr float samplerate = 48000;

	std::uniform_real_distribution<float> dist(0.f, 1.f);

	AllpassDiffuser diffuser(samplerate, rng);

	std::uniform_real_distribution<float> delay_dist(
		AllpassDiffuser::delay_bounds.first,
		AllpassDiffuser::delay_bounds.second
	);
	std::uniform_real_distribution<float> mod_dist(
		AllpassDiffuser::mod_bounds.first,
		AllpassDiffuser::mod_bounds.second
	);

	diffuser.set_delay(delay_dist(rng)*samplerate);
	diffuser.set_mod_rate(0.5*dist(rng));
	diffuser.set_mod_depth(mod_dist(rng)*samplerate);
	AllpassDiffuser::PushInfo info = {
		.stages = AllpassDiffuser::max_stages,
		.feedback = dist(rng),
		.interpolate = !(rng() & 0x80000000)
	};

	for (size_t i = 0; i < 10'000'000; ++i)
		diffuser.push(dist(rng), info);
}
