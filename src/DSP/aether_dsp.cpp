#include <algorithm>
#include <cmath>

#include "aether_dsp.hpp"

namespace {
	float dBtoGain(float db) noexcept {
		return std::pow(10.f, db/20.f);
	}
}

namespace Aether {
	DSP::DSP(float rate) :
		m_l_predelay(rate),
		m_r_predelay(rate),
		m_l_early_filters(rate),
		m_r_early_filters(rate),
		m_l_early_multitap(rate),
		m_r_early_multitap(rate),
		m_l_early_diffuser(rate, rng),
		m_r_early_diffuser(rate, rng),
		m_l_late_rev(rate, rng),
		m_r_late_rev(rate, rng),
		m_rate{rate}
	{}

	void DSP::map_uris(LV2_URID_Map*) noexcept {}

	void DSP::process(uint32_t n_samples) noexcept {

		// Early Reflections
		{ // Filters
			float cutoff = *ports.early_low_cut_cutoff;
			m_l_early_filters.highpass.set_cutoff(cutoff);
			m_r_early_filters.highpass.set_cutoff(cutoff);
		} {
			float cutoff = *ports.early_high_cut_cutoff;
			m_l_early_filters.lowpass.set_cutoff(cutoff);
			m_r_early_filters.lowpass.set_cutoff(cutoff);
		}

		{ // Multitap Delay
			float decay = *ports.early_tap_decay;
			m_l_early_multitap.set_decay(decay);
			m_r_early_multitap.set_decay(decay);
		} {
			float crossmix = *ports.seed_crossmix/200.f;
			m_l_early_multitap.set_seed_crossmix(1.f-crossmix);
			m_r_early_multitap.set_seed_crossmix(0.f+crossmix);
		} {
			uint32_t seed = static_cast<uint32_t>(*ports.tap_seed);
			m_l_early_multitap.set_seed(seed);
			m_r_early_multitap.set_seed(seed);
		}

		{ // Diffuser
			float delay = m_rate**ports.early_diffusion_delay/1000.f;
			m_l_early_diffuser.set_delay(delay);
			m_r_early_diffuser.set_delay(delay);
		} {
			float mod_depth = m_rate**ports.early_diffusion_mod_depth/1000.f;
			m_l_early_diffuser.set_mod_depth(mod_depth);
			m_r_early_diffuser.set_mod_depth(mod_depth);
		} {
			float rate = *ports.early_diffusion_mod_rate*m_rate;
			m_l_early_diffuser.set_mod_rate(rate);
			m_r_early_diffuser.set_mod_rate(rate);
		} {
			float crossmix = *ports.seed_crossmix/200.f;
			m_l_early_diffuser.set_seed_crossmix(1.f-crossmix);
			m_r_early_diffuser.set_seed_crossmix(0.f+crossmix);
		} {
			uint32_t seed = static_cast<uint32_t>(*ports.early_diffusion_seed);
			m_l_early_diffuser.set_seed(seed);
			m_r_early_diffuser.set_seed(seed);
		}

		// Late Reverberations
		{ // General
			float crossmix = *ports.seed_crossmix/200.f;
			m_l_late_rev.set_seed_crossmix(1.f-crossmix);
			m_r_late_rev.set_seed_crossmix(0.f+crossmix);
		}

		{ // Modulated Delay
			float delay = m_rate**ports.late_delay/1000.f;
			m_l_late_rev.set_delay(delay);
			m_r_late_rev.set_delay(delay);
		} {
			float mod_depth = m_rate**ports.late_delay_mod_depth/1000.f;
			m_l_late_rev.set_delay_mod_depth(mod_depth);
			m_r_late_rev.set_delay_mod_depth(mod_depth);
		} {
			float mod_rate = *ports.late_delay_mod_rate/m_rate;
			m_l_late_rev.set_delay_mod_rate(mod_rate);
			m_r_late_rev.set_delay_mod_rate(mod_rate);
		} {
			float feedback = *ports.late_delay_line_feedback;
			m_l_late_rev.set_delay_feedback(feedback);
			m_r_late_rev.set_delay_feedback(feedback);
		} {
			uint32_t seed = static_cast<uint32_t>(*ports.delay_seed);
			m_l_late_rev.set_delay_seed(seed);
			m_r_late_rev.set_delay_seed(seed);
		}

		{ // Diffuser
			float delay = m_rate**ports.late_diffusion_delay/1000.f;
			m_l_late_rev.set_diffusion_delay(delay);
			m_r_late_rev.set_diffusion_delay(delay);
		} {
			float depth = m_rate**ports.late_diffusion_mod_depth/1000.f;
			m_l_late_rev.set_diffusion_mod_depth(depth);
			m_r_late_rev.set_diffusion_mod_depth(depth);
		} {
			float rate = *ports.late_diffusion_mod_rate/m_rate;
			m_l_late_rev.set_diffusion_mod_rate(rate);
			m_r_late_rev.set_diffusion_mod_rate(rate);
		} {
			uint32_t seed = static_cast<uint32_t>(*ports.late_diffusion_seed);
			m_l_late_rev.set_diffusion_seed(seed);
			m_r_late_rev.set_diffusion_seed(seed);
		}

		{ // Filters
			float cutoff = *ports.late_low_shelf_cutoff;
			m_l_late_rev.set_low_shelf_cutoff(cutoff);
			m_r_late_rev.set_low_shelf_cutoff(cutoff);
		} {
			float gain = dBtoGain(*ports.late_low_shelf_gain);
			m_l_late_rev.set_low_shelf_gain(gain);
			m_r_late_rev.set_low_shelf_gain(gain);
		} {
			float cutoff = *ports.late_high_shelf_cutoff;
			m_l_late_rev.set_high_shelf_cutoff(cutoff);
			m_r_late_rev.set_high_shelf_cutoff(cutoff);
		} {
			float gain = dBtoGain(*ports.late_high_shelf_gain);
			m_l_late_rev.set_high_shelf_gain(gain);
			m_r_late_rev.set_high_shelf_gain(gain);
		} {
			float cutoff = *ports.late_high_cut_cutoff;
			m_l_late_rev.set_high_cut_cutoff(cutoff);
			m_r_late_rev.set_high_cut_cutoff(cutoff);
		}

		for (uint32_t sample = 0; sample < n_samples; ++sample) {
			// Dry
			float dry_left = ports.audio_in_left[sample];
			float dry_right = ports.audio_in_right[sample];

			{
				float dry_level = *ports.dry_level/100.f;
				ports.audio_out_left[sample] = dry_level*dry_left;
				ports.audio_out_right[sample] = dry_level*dry_right;
			}

			// Predelay
			float predelay_left = dry_left;
			float predelay_right = dry_right;
			{
				float crossover = *ports.crossover/200.f;
				predelay_left = std::lerp(dry_left, dry_right, crossover);
				predelay_right = std::lerp(dry_right, dry_left, crossover);

				// predelay in samples
				uint32_t delay = static_cast<uint32_t>(*ports.predelay/1000.f*m_rate);
				predelay_left = m_l_predelay.push(predelay_left, delay);
				predelay_right = m_r_predelay.push(predelay_right, delay);

				float predelay_level = *ports.predelay_level/100.f;
				ports.audio_out_left[sample] += predelay_level*predelay_left;
				ports.audio_out_right[sample] += predelay_level*predelay_right;
			}

			// Early Reflections
			float early_left = predelay_left;
			float early_right = predelay_right;
			{
				// Filtering
				if (*ports.early_low_cut_enabled > 0.f) {
					early_left = m_l_early_filters.highpass.push(early_left);
					early_right = m_r_early_filters.highpass.push(early_right);
				}

				if (*ports.early_high_cut_enabled > 0.f) {
					early_left = m_l_early_filters.lowpass.push(early_left);
					early_right = m_r_early_filters.lowpass.push(early_right);
				}

				{ // multitap delay
					uint32_t taps = static_cast<uint32_t>(*ports.early_taps);
					float length = *ports.early_tap_length/1000.f*m_rate;

					float multitap_left = m_l_early_multitap.push(early_left, taps, length);
					float multitap_right = m_r_early_multitap.push(early_right, taps, length);

					float tap_mix = *ports.early_tap_mix/100.f;
					early_left = std::lerp( early_left, multitap_left, tap_mix);
					early_right = std::lerp(early_right, multitap_right, tap_mix);
				}

				{ // allpass diffuser
					AllpassDiffuser::PushInfo info = {
						.stages = static_cast<uint32_t>(*ports.early_diffusion_stages),
						.feedback = *ports.early_diffusion_feedback,
						.interpolate = true
					};

					early_left = m_l_early_diffuser.push(early_left, info);
					early_right = m_r_early_diffuser.push(early_right, info);
				}

				float early_level = *ports.early_level/100.f;
				ports.audio_out_left[sample] += early_level*early_left;
				ports.audio_out_right[sample] += early_level*early_right;
			}

			// Late Reverberations
			float late_left = early_left;
			float late_right = early_right;
			{
				uint32_t lines = static_cast<uint32_t>(*ports.late_delay_lines);

				Delayline::PushInfo push_info = {
					.order = static_cast<Delayline::Order>(*ports.late_order),
					.diffuser_info = {
						.stages = static_cast<uint32_t>(*ports.late_diffusion_stages),
						.feedback = *ports.late_diffusion_feedback,
						.interpolate = *ports.interpolate > 0.f
					},
					.damping_info = {
						.ls_enable = *ports.late_low_shelf_enabled > 0.f,
						.hs_enable = *ports.late_high_shelf_enabled > 0.f,
						.hc_enable = *ports.late_high_cut_enabled > 0.f
					}
				};

				late_left = m_l_late_rev.push(late_left, lines, push_info);
				late_right = m_l_late_rev.push(late_right, lines, push_info);

				float late_level = *ports.late_level/100.f;
				ports.audio_out_left[sample] += late_level*late_left;
				ports.audio_out_right[sample] += late_level*late_right;
			}

			{
				float mix = *ports.mix/100.f;
				ports.audio_out_left[sample] = std::lerp(dry_left, ports.audio_out_left[sample], mix);
				ports.audio_out_right[sample] = std::lerp(dry_right, ports.audio_out_right[sample], mix);
			}
		}
	}
}
