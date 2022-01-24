#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string_view>

// LV2
#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>
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
		static constexpr std::string_view l_samples_URI = "#lSamples";
		static constexpr std::string_view r_samples_URI = "#rSamples";

		struct Ports {
			const LV2_Atom_Sequence* control;
			LV2_Atom_Sequence* notify;
			const float* audio_in_left;
			const float* audio_in_right;
			float* audio_out_left;
			float* audio_out_right;
		};

		template <class T>
		struct Parameters {
			T mix;

			// mixer
			T dry_level;
			T predelay_level;
			T early_level;
			T late_level;

			// Global
			T interpolate;

			// predelay
			T width;
			T predelay;

			// early
			// filtering
			T early_low_cut_enabled;
			T early_low_cut_cutoff;
			T early_high_cut_enabled;
			T early_high_cut_cutoff;
			// multitap delay
			T early_taps;
			T early_tap_length;
			T early_tap_mix;
			T early_tap_decay;
			// diffusion
			T early_diffusion_stages;
			T early_diffusion_delay;
			T early_diffusion_mod_depth;
			T early_diffusion_mod_rate;
			T early_diffusion_feedback;

			// late
			T late_order;
			T late_delay_lines;
			// delay line
			T late_delay;
			T late_delay_mod_depth;
			T late_delay_mod_rate;
			T late_delay_line_feedback;
			// diffusion
			T late_diffusion_stages;
			T late_diffusion_delay;
			T late_diffusion_mod_depth;
			T late_diffusion_mod_rate;
			T late_diffusion_feedback;
			// Filter
			T late_low_shelf_enabled;
			T late_low_shelf_cutoff;
			T late_low_shelf_gain;
			T late_high_shelf_enabled;
			T late_high_shelf_cutoff;
			T late_high_shelf_gain;
			T late_high_cut_enabled;
			T late_high_cut_cutoff;

			// Seed
			T seed_crossmix;
			T tap_seed;
			T early_diffusion_seed;
			T delay_seed;
			T late_diffusion_seed;

			// Distortion
			T early_diffusion_drive;
			T late_diffusion_drive;

			T& operator[](size_t idx) noexcept { return data()[idx]; }
			const T& operator[](size_t idx) const noexcept { return data()[idx]; }

			T* data() noexcept { return reinterpret_cast<T*>(this); }
			const T* data() const noexcept { return reinterpret_cast<const T*>(this); }

			T* begin() noexcept { return data(); }
			const T* begin() const noexcept { return data(); }

			T* end() noexcept { return data() + size(); }
			const T* end() const noexcept { return data() + size(); }

			static constexpr size_t size() noexcept { return sizeof(Parameters<T>) / sizeof(T); }
		};

		Ports ports = {};

		Parameters<float> params = {};
		Parameters<float> param_targets = {};
		Parameters<float> param_smooth = {};
		Parameters<bool> params_modified = {};
		std::array<const float*, 47> param_ports = {};

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
			LV2_URID l_samples;
			LV2_URID r_samples;
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

		static size_t sizeof_peak_data_atom() noexcept;
		static size_t sizeof_sample_data_atom(uint32_t n_samples) noexcept;
		void write_sample_data_atom(
			int channel,
			int rate,
			uint32_t n_samples,
			const float* l_samples,
			const float* r_samples
		) noexcept;

		// Updates param_targets
		void update_parameter_targets() noexcept;
		// Updates params & params_modified then calls apply_parameters
		void update_parameters() noexcept;
		// Applies changes in params & params_modified to internal state
		void apply_parameters() noexcept;
	};
}
