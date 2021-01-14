#pragma once

#include <cstddef>
#include <random>

// LV2
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#include "utils/random.hpp"

#include "delay.hpp"
#include "filters.hpp"
#include "diffuser.hpp"
#include "delayline.hpp"

namespace Aether {

	class DSP {
	public:
		static constexpr const char* URI = "http://github.com/Dougal-s/Aether";

		struct Ports {
			const LV2_Atom_Sequence* control;
			LV2_Atom_Sequence* notify;
			const float* audio_in_left;
			const float* audio_in_right;
			float* audio_out_left;
			float* audio_out_right;
			const float* mix;

			// mixer
			const float* dry_level;
			const float* predelay_level;
			const float* early_level;
			const float* late_level;

			// Global
			const float* interpolate;

			// predelay
			const float* crossover;
			const float* predelay;

			// early
			// filtering
			const float* early_low_cut_enabled;
			const float* early_low_cut_cutoff;
			const float* early_high_cut_enabled;
			const float* early_high_cut_cutoff;
			// multitap delay
			const float* early_taps;
			const float* early_tap_length;
			const float* early_tap_mix;
			const float* early_tap_decay;
			// diffusion
			const float* early_diffusion_stages;
			const float* early_diffusion_delay;
			const float* early_diffusion_mod_depth;
			const float* early_diffusion_mod_rate;
			const float* early_diffusion_feedback;

			// late
			const float* late_order;
			const float* late_delay_lines;
			// delay line
			const float* late_delay;
			const float* late_delay_mod_depth;
			const float* late_delay_mod_rate;
			const float* late_delay_line_feedback;
			// diffusion
			const float* late_diffusion_stages;
			const float* late_diffusion_delay;
			const float* late_diffusion_mod_depth;
			const float* late_diffusion_mod_rate;
			const float* late_diffusion_feedback;
			// Filter
			const float* late_low_shelf_enabled;
			const float* late_low_shelf_cutoff;
			const float* late_low_shelf_gain;
			const float* late_high_shelf_enabled;
			const float* late_high_shelf_cutoff;
			const float* late_high_shelf_gain;
			const float* late_high_cut_enabled;
			const float* late_high_cut_cutoff;

			// Seed
			const float* seed_crossmix;
			const float* tap_seed;
			const float* early_diffusion_seed;
			const float* delay_seed;
			const float* late_diffusion_seed;
		};

		Ports ports = {};

		/*
			Member Functions
		*/

		DSP(float rate);
		~DSP() = default;

		void map_uris(LV2_URID_Map* map) noexcept;

		void process(uint32_t n_samples) noexcept;

	private:
		Random::Xorshift64s rng{std::random_device{}()};

		struct URIs {};
		URIs uris = {};

		// Predelay
		Delay m_l_predelay;
		Delay m_r_predelay;

		// Early
		struct Filters {
			Filters(float rate) : lowpass(rate), highpass(rate) {}

			Lowpass6dB lowpass;
			Highpass6dB highpass;
		};

		Filters m_l_early_filters;
		Filters m_r_early_filters;

		MultitapDelay m_l_early_multitap;
		MultitapDelay m_r_early_multitap;

		AllpassDiffuser m_l_early_diffuser;
		AllpassDiffuser m_r_early_diffuser;

		// Late
		LateRev m_l_late_rev;
		LateRev m_r_late_rev;

		float m_rate;
	};
}
