#include <vector>

#include <gtest/gtest.h>

#include "DSP/aether_dsp.hpp"

TEST(output, silence) {
	static constexpr size_t buffer_size = 1024;
	std::vector<float> l_buf(buffer_size, 0.f);
	std::vector<float> r_buf(buffer_size, 0.f);

	Aether::DSP dsp(48000);
	dsp.ports.audio_in_left = l_buf.data();
	dsp.ports.audio_in_right = r_buf.data();
	dsp.ports.audio_out_left = l_buf.data();
	dsp.ports.audio_out_right = r_buf.data();

	float mix = 0.f;
	float dry_level = 0.f;
	float predelay_level = 0.f;
	float early_level = 0.f;
	float late_level = 0.f;
	float interpolate = 0.f;
	float width = 1.f;
	float predelay = 0.f;
	float early_low_cut_enabled = 0.f;
	float early_low_cut_cutoff = 0.f;
	float early_high_cut_enabled = 0.f;
	float early_high_cut_cutoff = 0.f;
	float early_taps = 5.f;
	float early_tap_length = 0.f;
	float early_tap_mix = 0.f;
	float early_tap_decay = 0.f;
	float early_diffusion_stages = 0.f;
	float early_diffusion_delay = 50.f;
	float early_diffusion_mod_depth = 0.f;
	float early_diffusion_mod_rate = 0.f;
	float early_diffusion_feedback = 0.f;
	float late_order = 0.f;
	float late_delay_lines = 5.f;
	float late_delay = 5.f;
	float late_delay_mod_depth = 0.f;
	float late_delay_mod_rate = 0.f;
	float late_delay_line_feedback = 0.f;
	float late_diffusion_stages = 0.f;
	float late_diffusion_delay = 50.f;
	float late_diffusion_mod_depth = 0.f;
	float late_diffusion_mod_rate = 0.f;
	float late_diffusion_feedback = 0.f;
	float late_low_shelf_enabled = 0.f;
	float late_low_shelf_cutoff = 0.f;
	float late_low_shelf_gain = 0.f;
	float late_high_shelf_enabled = 0.f;
	float late_high_shelf_cutoff = 0.f;
	float late_high_shelf_gain = 0.f;
	float late_high_cut_enabled = 0.f;
	float late_high_cut_cutoff = 0.f;
	float seed_crossmix = 0.f;
	float tap_seed = 10.f;
	float early_diffusion_seed = 10.f;
	float delay_seed = 10.f;
	float late_diffusion_seed = 10.f;

	dsp.param_ports = {
		&mix,
		&dry_level,
		&predelay_level,
		&early_level,
		&late_level,
		&interpolate,
		&width,
		&predelay,
		&early_low_cut_enabled,
		&early_low_cut_cutoff,
		&early_high_cut_enabled,
		&early_high_cut_cutoff,
		&early_taps,
		&early_tap_length,
		&early_tap_mix,
		&early_tap_decay,
		&early_diffusion_stages,
		&early_diffusion_delay,
		&early_diffusion_mod_depth,
		&early_diffusion_mod_rate,
		&early_diffusion_feedback,
		&late_order,
		&late_delay_lines,
		&late_delay,
		&late_delay_mod_depth,
		&late_delay_mod_rate,
		&late_delay_line_feedback,
		&late_diffusion_stages,
		&late_diffusion_delay,
		&late_diffusion_mod_depth,
		&late_diffusion_mod_rate,
		&late_diffusion_feedback,
		&late_low_shelf_enabled,
		&late_low_shelf_cutoff,
		&late_low_shelf_gain,
		&late_high_shelf_enabled,
		&late_high_shelf_cutoff,
		&late_high_shelf_gain,
		&late_high_cut_enabled,
		&late_high_cut_cutoff,
		&seed_crossmix,
		&tap_seed,
		&early_diffusion_seed,
		&delay_seed,
		&late_diffusion_seed
	};

	dsp.process(buffer_size);

	EXPECT_FLOAT_EQ(std::accumulate(l_buf.begin(), l_buf.end(), 0.f), 0.f);
	EXPECT_FLOAT_EQ(std::accumulate(r_buf.begin(), r_buf.end(), 0.f), 0.f);
}
