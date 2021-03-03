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
	dsp.ports.mix = &mix;
	float dry_level = 0.f;
	dsp.ports.dry_level = &dry_level;
	float predelay_level = 0.f;
	dsp.ports.predelay_level = &predelay_level;
	float early_level = 0.f;
	dsp.ports.early_level = &early_level;
	float late_level = 0.f;
	dsp.ports.late_level = &late_level;
	float interpolate = 0.f;
	dsp.ports.interpolate = &interpolate;
	float width = 1.f;
	dsp.ports.width = &width;
	float predelay = 0.f;
	dsp.ports.predelay = &predelay;
	float early_low_cut_enabled = 0.f;
	dsp.ports.early_low_cut_enabled = &early_low_cut_enabled;
	float early_low_cut_cutoff = 0.f;
	dsp.ports.early_low_cut_cutoff = &early_low_cut_cutoff;
	float early_high_cut_enabled = 0.f;
	dsp.ports.early_high_cut_enabled = &early_high_cut_enabled;
	float early_high_cut_cutoff = 0.f;
	dsp.ports.early_high_cut_cutoff = &early_high_cut_cutoff;
	float early_taps = 5.f;
	dsp.ports.early_taps = &early_taps;
	float early_tap_length = 0.f;
	dsp.ports.early_tap_length = &early_tap_length;
	float early_tap_mix = 0.f;
	dsp.ports.early_tap_mix = &early_tap_mix;
	float early_tap_decay = 0.f;
	dsp.ports.early_tap_decay = &early_tap_decay;
	float early_diffusion_stages = 0.f;
	dsp.ports.early_diffusion_stages = &early_diffusion_stages;
	float early_diffusion_delay = 50.f;
	dsp.ports.early_diffusion_delay = &early_diffusion_delay;
	float early_diffusion_mod_depth = 0.f;
	dsp.ports.early_diffusion_mod_depth = &early_diffusion_mod_depth;
	float early_diffusion_mod_rate = 0.f;
	dsp.ports.early_diffusion_mod_rate = &early_diffusion_mod_rate;
	float early_diffusion_feedback = 0.f;
	dsp.ports.early_diffusion_feedback = &early_diffusion_feedback;
	float late_order = 0.f;
	dsp.ports.late_order = &late_order;
	float late_delay_lines = 5.f;
	dsp.ports.late_delay_lines = &late_delay_lines;
	float late_delay = 5.f;
	dsp.ports.late_delay = &late_delay;
	float late_delay_mod_depth = 0.f;
	dsp.ports.late_delay_mod_depth = &late_delay_mod_depth;
	float late_delay_mod_rate = 0.f;
	dsp.ports.late_delay_mod_rate = &late_delay_mod_rate;
	float late_delay_line_feedback = 0.f;
	dsp.ports.late_delay_line_feedback = &late_delay_line_feedback;
	float late_diffusion_stages = 0.f;
	dsp.ports.late_diffusion_stages = &late_diffusion_stages;
	float late_diffusion_delay = 50.f;
	dsp.ports.late_diffusion_delay = &late_diffusion_delay;
	float late_diffusion_mod_depth = 0.f;
	dsp.ports.late_diffusion_mod_depth = &late_diffusion_mod_depth;
	float late_diffusion_mod_rate = 0.f;
	dsp.ports.late_diffusion_mod_rate = &late_diffusion_mod_rate;
	float late_diffusion_feedback = 0.f;
	dsp.ports.late_diffusion_feedback = &late_diffusion_feedback;
	float late_low_shelf_enabled = 0.f;
	dsp.ports.late_low_shelf_enabled = &late_low_shelf_enabled;
	float late_low_shelf_cutoff = 0.f;
	dsp.ports.late_low_shelf_cutoff = &late_low_shelf_cutoff;
	float late_low_shelf_gain = 0.f;
	dsp.ports.late_low_shelf_gain = &late_low_shelf_gain;
	float late_high_shelf_enabled = 0.f;
	dsp.ports.late_high_shelf_enabled = &late_high_shelf_enabled;
	float late_high_shelf_cutoff = 0.f;
	dsp.ports.late_high_shelf_cutoff = &late_high_shelf_cutoff;
	float late_high_shelf_gain = 0.f;
	dsp.ports.late_high_shelf_gain = &late_high_shelf_gain;
	float late_high_cut_enabled = 0.f;
	dsp.ports.late_high_cut_enabled = &late_high_cut_enabled;
	float late_high_cut_cutoff = 0.f;
	dsp.ports.late_high_cut_cutoff = &late_high_cut_cutoff;
	float seed_crossmix = 0.f;
	dsp.ports.seed_crossmix = &seed_crossmix;
	float tap_seed = 10.f;
	dsp.ports.tap_seed = &tap_seed;
	float early_diffusion_seed = 10.f;
	dsp.ports.early_diffusion_seed = &early_diffusion_seed;
	float delay_seed = 10.f;
	dsp.ports.delay_seed = &delay_seed;
	float late_diffusion_seed = 10.f;
	dsp.ports.late_diffusion_seed = &late_diffusion_seed;

	dsp.process(buffer_size);

	EXPECT_FLOAT_EQ(std::reduce(l_buf.begin(), l_buf.end())/l_buf.size(), 0.f);
	EXPECT_FLOAT_EQ(std::reduce(r_buf.begin(), r_buf.end())/r_buf.size(), 0.f);
}
