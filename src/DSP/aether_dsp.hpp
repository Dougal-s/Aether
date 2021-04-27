#pragma once

#include <array>
#include <cstddef>
#include <random>
#include <string_view>

// LV2
#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>
#include <lv2/core/lv2.h>
#include <lv2/atom/forge.h>

#include "utils/random.hpp"

#include "delay.hpp"
#include "filters.hpp"
#include "diffuser.hpp"
#include "delayline.hpp"

namespace Aether {

	class DSP {
	public:
		static constexpr std::string_view URI = "http://github.com/Dougal-s/Aether";

		static constexpr std::string_view ui_open_URI = "#uiOpen";
		static constexpr std::string_view ui_close_URI = "#uiClose";

		static constexpr std::string_view peak_data_URI = "#peakData";
		static constexpr std::string_view sample_count_URI = "#sampleCount";
		static constexpr std::string_view peaks_URI = "#peaks";

		static constexpr std::string_view sample_data_URI = "#sampleData";
		static constexpr std::string_view rate_URI = "#rate";
		static constexpr std::string_view channel_URI = "#channel";
		static constexpr std::string_view samples_URI = "#samples";

		struct Ports {
			const LV2_Atom_Sequence* control;
			LV2_Atom_Sequence* notify;
			const float* audio_in_left;
			const float* audio_in_right;
			float* audio_out_left;
			float* audio_out_right;
		};

		struct Parameters {
			float mix;

			// mixer
			float dry_level;
			float predelay_level;
			float early_level;
			float late_level;

			// Global
			float interpolate;

			// predelay
			float width;
			float predelay;

			// early
			// filtering
			float early_low_cut_enabled;
			float early_low_cut_cutoff;
			float early_high_cut_enabled;
			float early_high_cut_cutoff;
			// multitap delay
			float early_taps;
			float early_tap_length;
			float early_tap_mix;
			float early_tap_decay;
			// diffusion
			float early_diffusion_stages;
			float early_diffusion_delay;
			float early_diffusion_mod_depth;
			float early_diffusion_mod_rate;
			float early_diffusion_feedback;

			// late
			float late_order;
			float late_delay_lines;
			// delay line
			float late_delay;
			float late_delay_mod_depth;
			float late_delay_mod_rate;
			float late_delay_line_feedback;
			// diffusion
			float late_diffusion_stages;
			float late_diffusion_delay;
			float late_diffusion_mod_depth;
			float late_diffusion_mod_rate;
			float late_diffusion_feedback;
			// Filter
			float late_low_shelf_enabled;
			float late_low_shelf_cutoff;
			float late_low_shelf_gain;
			float late_high_shelf_enabled;
			float late_high_shelf_cutoff;
			float late_high_shelf_gain;
			float late_high_cut_enabled;
			float late_high_cut_cutoff;

			// Seed
			float seed_crossmix;
			float tap_seed;
			float early_diffusion_seed;
			float delay_seed;
			float late_diffusion_seed;
		};

		Ports ports = {};

		std::array<const float*, 45> param_ports = {};
		union {
			Parameters param_smooth_named;
			std::array<float, 45> param_smooth = {};
		};

		union {
			Parameters params;
			std::array<float, 45> params_arr;
		};

		/*
			Member Functions
		*/

		DSP(float rate);
		~DSP() = default;

		void map_uris(LV2_URID_Map* map) noexcept;

		void process(uint32_t n_samples) noexcept;

	private:
		Random::Xorshift64s rng{std::random_device{}()};

		struct URIs {
			LV2_URID atom_Object;
			LV2_URID atom_Float;
			// custom uris
			LV2_URID ui_open;
			LV2_URID ui_close;

			LV2_URID peak_data;
			LV2_URID sample_count;
			LV2_URID peaks;

			LV2_URID sample_data;
			LV2_URID rate;
			LV2_URID channel;
			LV2_URID samples;
		};

		URIs uris = {};
		LV2_Atom_Forge atom_forge = {};

		// Predelay
		Delay m_l_predelay;
		Delay m_r_predelay;

		// Early
		struct Filters {
			Filters(float rate) : lowpass(rate), highpass(rate) {}

			Lowpass6dB<float> lowpass;
			Highpass6dB<float> highpass;
		};

		Filters m_l_early_filters;
		Filters m_r_early_filters;

		MultitapDelay m_l_early_multitap;
		MultitapDelay m_r_early_multitap;

		AllpassDiffuser<float> m_l_early_diffuser;
		AllpassDiffuser<float> m_r_early_diffuser;

		// Late
		LateRev m_l_late_rev;
		LateRev m_r_late_rev;

		float m_rate;

		// send audio data if ui is open
		bool ui_open = false;

		void write_sample_data_atom(int channel, int rate, float* data, uint32_t n_samples);

		void update_parameters();
	};
}
