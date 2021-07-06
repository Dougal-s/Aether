#include <algorithm>
#include <cmath>

// Lv2
#include <lv2/atom/util.h>

#include "aether_dsp.hpp"
#include "../common/parameters.hpp"
#include "../common/utils.hpp"

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
	{
		for (size_t i = 6; float& param : params_arr)
			param = parameter_infos[i++].dflt;

		param_smooth_named.mix = 50.f;

		param_smooth_named.dry_level = 50.f;
		param_smooth_named.predelay_level = 50.f;
		param_smooth_named.early_level = 50.f;
		param_smooth_named.late_level = 50.f;

		param_smooth_named.width = 50.f;
		param_smooth_named.predelay = 5000.f;

		param_smooth_named.early_tap_mix = 50.f;
		param_smooth_named.early_tap_decay = 25.f;
		param_smooth_named.early_tap_length = 4000.f;

		param_smooth_named.early_diffusion_delay = 5000.f;
		param_smooth_named.early_diffusion_mod_depth = 1000.f;
		param_smooth_named.early_diffusion_feedback = 500.f;

		param_smooth_named.late_delay = 5000.f;
		param_smooth_named.late_delay_mod_depth = 1000.f;
		param_smooth_named.late_delay_line_feedback = 50.f;

		param_smooth_named.late_diffusion_delay = 5000.f;
		param_smooth_named.late_diffusion_mod_depth = 2000.f;
		param_smooth_named.late_diffusion_feedback = 500.f;

		param_smooth_named.seed_crossmix = 5000.f;

		for (float& smooth : param_smooth) {
			constexpr float pi = constants::pi_v<float>;
			if (smooth != 0.f)
				smooth = std::exp(-2*pi / (0.0001f*smooth * rate));
		}
	}

	void DSP::map_uris(LV2_URID_Map* map) noexcept {
		lv2_atom_forge_init(&atom_forge, map);
		uris.atom_Object = map->map(map->handle, LV2_ATOM__Object);
		uris.atom_Float = map->map(map->handle, LV2_ATOM__Float);

		uris.ui_open = map->map(map->handle, join_v<URI, ui_open_URI>);
		uris.ui_close = map->map(map->handle, join_v<URI, ui_close_URI>);

		uris.peak_data = map->map(map->handle, join_v<URI, peak_data_URI>);
		uris.sample_count = map->map(map->handle, join_v<URI, sample_count_URI>);
		uris.peaks = map->map(map->handle, join_v<URI, peaks_URI>);

		uris.sample_data = map->map(map->handle, join_v<URI, sample_data_URI>);
		uris.rate        = map->map(map->handle, join_v<URI, rate_URI>);
		uris.channel     = map->map(map->handle, join_v<URI, channel_URI>);
		uris.l_samples   = map->map(map->handle, join_v<URI, l_samples_URI>);
		uris.r_samples   = map->map(map->handle, join_v<URI, r_samples_URI>);
	}

	void DSP::process(uint32_t n_samples) noexcept {
		if (ports.control) {
			LV2_ATOM_SEQUENCE_FOREACH(ports.control, event) {
				if (event->body.type == uris.atom_Object) {
					const auto obj = reinterpret_cast<LV2_Atom_Object*>(&event->body);
					if (obj->body.otype == uris.ui_open)
						ui_open = true;
					else if (obj->body.otype == uris.ui_close)
						ui_open = false;
				}
			}
		}

		LV2_Atom_Forge_Frame seq_frame;
		bool notify_ui = ports.notify ? ui_open : false;
		if (notify_ui) {
			const size_t seq_capacity = ports.notify->atom.size;
			size_t min_seq_capacity = sizeof(LV2_Atom_Sequence)
				+ sizeof_peak_data_atom() + 2*sizeof_sample_data_atom(n_samples);
			notify_ui = seq_capacity >= min_seq_capacity;

			if (notify_ui) {
				lv2_atom_forge_set_buffer(&atom_forge, reinterpret_cast<uint8_t*>(ports.notify), seq_capacity);
				lv2_atom_forge_sequence_head(&atom_forge, &seq_frame, 0);
				write_sample_data_atom(0, static_cast<int>(m_rate), n_samples, ports.audio_in_left, ports.audio_in_right);
			}
		}

		std::pair<float, float> peak_dry = {0,0},
			peak_dry_stage = {0,0},
			peak_predelay_stage = {0,0},
			peak_early_stage = {0,0},
			peak_late_stage = {0,0},
			peak_out = {0,0};

		for (uint32_t sample = 0; sample < n_samples; ++sample) {
			update_parameters();

			// Dry
			float dry_level = params.dry_level/100.f;
			float dry_left = ports.audio_in_left[sample];
			float dry_right = ports.audio_in_right[sample];
			{
				ports.audio_out_left[sample] = dry_level*dry_left;
				ports.audio_out_right[sample] = dry_level*dry_right;
			}

			// Predelay
			float predelay_level = params.predelay_level/100.f;
			float predelay_left = dry_left;
			float predelay_right = dry_right;
			{
				float width = 0.5f-params.width/200.f;
				predelay_left = std::lerp(dry_left, dry_right, width);
				predelay_right = std::lerp(dry_right, dry_left, width);

				// predelay in samples
				uint32_t delay = static_cast<uint32_t>(params.predelay/1000.f*m_rate);
				predelay_left = m_l_predelay.push(predelay_left, delay);
				predelay_right = m_r_predelay.push(predelay_right, delay);

				ports.audio_out_left[sample] += predelay_level*predelay_left;
				ports.audio_out_right[sample] += predelay_level*predelay_right;
			}

			// Early Reflections
			float early_level = params.early_level/100.f;
			float early_left = predelay_left;
			float early_right = predelay_right;
			{
				// Filtering
				if (params.early_low_cut_enabled > 0.f) {
					early_left = m_l_early_filters.highpass.push(early_left);
					early_right = m_r_early_filters.highpass.push(early_right);
				}

				if (params.early_high_cut_enabled > 0.f) {
					early_left = m_l_early_filters.lowpass.push(early_left);
					early_right = m_r_early_filters.lowpass.push(early_right);
				}

				{ // multitap delay
					uint32_t taps = static_cast<uint32_t>(params.early_taps);
					float length = params.early_tap_length/1000.f*m_rate;

					float multitap_left = m_l_early_multitap.push(early_left, taps, length);
					float multitap_right = m_r_early_multitap.push(early_right, taps, length);

					float tap_mix = params.early_tap_mix/100.f;
					early_left = std::lerp( early_left, multitap_left, tap_mix);
					early_right = std::lerp(early_right, multitap_right, tap_mix);
				}

				{ // allpass diffuser
					AllpassDiffuser<float>::PushInfo info = {
						.stages = static_cast<uint32_t>(params.early_diffusion_stages),
						.feedback = params.early_diffusion_feedback,
						.interpolate = true
					};

					early_left = m_l_early_diffuser.push(early_left, info);
					early_right = m_r_early_diffuser.push(early_right, info);
				}

				ports.audio_out_left[sample] += early_level*early_left;
				ports.audio_out_right[sample] += early_level*early_right;
			}

			// Late Reverberations
			float late_level = params.late_level/100.f;
			float late_left = early_left;
			float late_right = early_right;
			{
				uint32_t lines = static_cast<uint32_t>(params.late_delay_lines);

				Delayline::PushInfo push_info = {
					.order = static_cast<Delayline::Order>(params.late_order),
					.diffuser_info = {
						.stages = static_cast<uint32_t>(params.late_diffusion_stages),
						.feedback = params.late_diffusion_feedback,
						.interpolate = params.interpolate > 0.f
					},
					.damping_info = {
						.ls_enable = params.late_low_shelf_enabled > 0.f,
						.hs_enable = params.late_high_shelf_enabled > 0.f,
						.hc_enable = params.late_high_cut_enabled > 0.f
					}
				};

				late_left = m_l_late_rev.push(late_left, lines, push_info);
				late_right = m_l_late_rev.push(late_right, lines, push_info);
				ports.audio_out_left[sample] += late_level*late_left;
				ports.audio_out_right[sample] += late_level*late_right;
			}

			{
				float mix = params.mix/100.f;
				ports.audio_out_left[sample] = std::lerp(dry_left, ports.audio_out_left[sample], mix);
				ports.audio_out_right[sample] = std::lerp(dry_right, ports.audio_out_right[sample], mix);
			}

			if (notify_ui) {
				peak_dry.first = std::max(peak_dry.first, std::abs(dry_left));
				peak_dry.second = std::max(peak_dry.second, std::abs(dry_right));
				peak_dry_stage.first = std::max(peak_dry_stage.first, std::abs(dry_left*dry_level));
				peak_dry_stage.second = std::max(peak_dry_stage.second, std::abs(dry_right*dry_level));
				peak_predelay_stage.first = std::max(peak_predelay_stage.first, std::abs(predelay_left*predelay_level));
				peak_predelay_stage.second = std::max(peak_predelay_stage.second, std::abs(predelay_right*predelay_level));
				peak_early_stage.first = std::max(peak_early_stage.first, std::abs(early_left*early_level));
				peak_early_stage.second = std::max(peak_early_stage.second, std::abs(early_right*early_level));
				peak_late_stage.first = std::max(peak_late_stage.first, std::abs(late_left*late_level));
				peak_late_stage.second = std::max(peak_late_stage.second, std::abs(late_right*late_level));
				peak_out.first = std::max(peak_out.first, std::abs(ports.audio_out_left[sample]));
				peak_out.second = std::max(peak_out.second, std::abs(ports.audio_out_right[sample]));
			}
		}
		if (notify_ui) {
			// write peak data
			lv2_atom_forge_frame_time(&atom_forge, 0);
			{
				LV2_Atom_Forge_Frame obj_frame;
				lv2_atom_forge_object(&atom_forge, &obj_frame, 0, uris.peak_data);

				lv2_atom_forge_key(&atom_forge, uris.sample_count);
				lv2_atom_forge_int(&atom_forge, static_cast<int32_t>(n_samples));

				const std::array<float, 12> peaks = {
					peak_dry.first				, peak_dry.second,
					peak_dry_stage.first		, peak_dry_stage.second,
					peak_predelay_stage.first	, peak_predelay_stage.second,
					peak_early_stage.first		, peak_early_stage.second,
					peak_late_stage.first		, peak_late_stage.second,
					peak_out.first				, peak_out.second
				};

				lv2_atom_forge_key(&atom_forge, uris.peaks);
				lv2_atom_forge_vector(&atom_forge, sizeof(float), uris.atom_Float, peaks.size(), peaks.data());

				lv2_atom_forge_pop(&atom_forge, &obj_frame);
			}

			// write sample data
			write_sample_data_atom(1, static_cast<int>(m_rate), n_samples, ports.audio_out_left, ports.audio_out_right);
			lv2_atom_forge_pop(&atom_forge, &seq_frame);
		}
	}

	size_t DSP::sizeof_peak_data_atom() noexcept {
		return sizeof(LV2_Atom_Event) // timestamp
			+ sizeof(LV2_Atom_Object_Body) // object header
			+ sizeof(LV2_Atom_Property_Body) + sizeof(int32_t) // sample_count
			+ sizeof(LV2_Atom_Property_Body) + sizeof(LV2_Atom_Vector_Body)
				+ 12*sizeof(float); // peak data
	}

	size_t DSP::sizeof_sample_data_atom(uint32_t n_samples) noexcept {
		return sizeof(LV2_Atom_Event) // timestamp
			+ sizeof(LV2_Atom_Object_Body) // object header
			+ sizeof(LV2_Atom_Property_Body) + sizeof(int32_t) //samplerate
			+ sizeof(LV2_Atom_Property_Body) + sizeof(int32_t) // channel no.
			+ sizeof(LV2_Atom_Property_Body) + sizeof(LV2_Atom_Vector_Body)
				+ n_samples*sizeof(float) // left channel
			+ sizeof(LV2_Atom_Property_Body) + sizeof(LV2_Atom_Vector_Body)
				+ n_samples*sizeof(float); // right channel
	}

	void DSP::write_sample_data_atom(
		int channel,
		int rate,
		uint32_t n_samples,
		const float* l_samples,
		const float* r_samples
	) noexcept {
		lv2_atom_forge_frame_time(&atom_forge, 0);
		{
			LV2_Atom_Forge_Frame obj_frame;
			lv2_atom_forge_object(&atom_forge, &obj_frame, 0, uris.sample_data);

			lv2_atom_forge_key(&atom_forge, uris.rate);
			lv2_atom_forge_int(&atom_forge, rate);

			lv2_atom_forge_key(&atom_forge, uris.channel);
			lv2_atom_forge_int(&atom_forge, channel);

			lv2_atom_forge_key(&atom_forge, uris.l_samples);
			lv2_atom_forge_vector(&atom_forge, sizeof(float), uris.atom_Float, n_samples, l_samples);
			lv2_atom_forge_key(&atom_forge, uris.r_samples);
			lv2_atom_forge_vector(&atom_forge, sizeof(float), uris.atom_Float, n_samples, r_samples);

			lv2_atom_forge_pop(&atom_forge, &obj_frame);
		}
	}

	void DSP::update_parameters() {
		for (size_t p = 0; p < param_ports.size(); ++p) {
			float new_val = std::clamp(
				*param_ports[p],
				parameter_infos[p+6].min,
				parameter_infos[p+6].max
			);

			params_arr[p] = std::lerp(new_val, params_arr[p], param_smooth[p]);
		}

		// Early Reflections
		{ // Filters
			float cutoff = params.early_low_cut_cutoff;
			m_l_early_filters.highpass.set_cutoff(cutoff);
			m_r_early_filters.highpass.set_cutoff(cutoff);
		} {
			float cutoff = params.early_high_cut_cutoff;
			m_l_early_filters.lowpass.set_cutoff(cutoff);
			m_r_early_filters.lowpass.set_cutoff(cutoff);
		}

		{ // Multitap Delay
			float decay = params.early_tap_decay;
			m_l_early_multitap.set_decay(decay);
			m_r_early_multitap.set_decay(decay);
		} {
			float crossmix = params.seed_crossmix/200.f;
			m_l_early_multitap.set_seed_crossmix(1.f-crossmix);
			m_r_early_multitap.set_seed_crossmix(0.f+crossmix);
		} {
			uint32_t seed = static_cast<uint32_t>(params.tap_seed);
			m_l_early_multitap.set_seed(seed);
			m_r_early_multitap.set_seed(seed);
		}

		{ // Diffuser
			float delay = m_rate*params.early_diffusion_delay/1000.f;
			m_l_early_diffuser.set_delay(delay);
			m_r_early_diffuser.set_delay(delay);
		} {
			float mod_depth = m_rate*params.early_diffusion_mod_depth/1000.f;
			m_l_early_diffuser.set_mod_depth(mod_depth);
			m_r_early_diffuser.set_mod_depth(mod_depth);
		} {
			float rate = params.early_diffusion_mod_rate/m_rate;
			m_l_early_diffuser.set_mod_rate(rate);
			m_r_early_diffuser.set_mod_rate(rate);
		} {
			float crossmix = params.seed_crossmix/200.f;
			m_l_early_diffuser.set_seed_crossmix(1.f-crossmix);
			m_r_early_diffuser.set_seed_crossmix(0.f+crossmix);
		} {
			uint32_t seed = static_cast<uint32_t>(params.early_diffusion_seed);
			m_l_early_diffuser.set_seed(seed);
			m_r_early_diffuser.set_seed(seed);
		}

		// Late Reverberations
		{ // General
			float crossmix = params.seed_crossmix/200.f;
			m_l_late_rev.set_seed_crossmix(1.f-crossmix);
			m_r_late_rev.set_seed_crossmix(0.f+crossmix);
		}

		{ // Modulated Delay
			float delay = m_rate*params.late_delay/1000.f;
			m_l_late_rev.set_delay(delay);
			m_r_late_rev.set_delay(delay);
		} {
			float mod_depth = m_rate*params.late_delay_mod_depth/1000.f;
			m_l_late_rev.set_delay_mod_depth(mod_depth);
			m_r_late_rev.set_delay_mod_depth(mod_depth);
		} {
			float mod_rate = params.late_delay_mod_rate/m_rate;
			m_l_late_rev.set_delay_mod_rate(mod_rate);
			m_r_late_rev.set_delay_mod_rate(mod_rate);
		} {
			float feedback = params.late_delay_line_feedback;
			m_l_late_rev.set_delay_feedback(feedback);
			m_r_late_rev.set_delay_feedback(feedback);
		} {
			uint32_t seed = static_cast<uint32_t>(params.delay_seed);
			m_l_late_rev.set_delay_seed(seed);
			m_r_late_rev.set_delay_seed(seed);
		}

		{ // Diffuser
			float delay = m_rate*params.late_diffusion_delay/1000.f;
			m_l_late_rev.set_diffusion_delay(delay);
			m_r_late_rev.set_diffusion_delay(delay);
		} {
			float depth = m_rate*params.late_diffusion_mod_depth/1000.f;
			m_l_late_rev.set_diffusion_mod_depth(depth);
			m_r_late_rev.set_diffusion_mod_depth(depth);
		} {
			float rate = params.late_diffusion_mod_rate/m_rate;
			m_l_late_rev.set_diffusion_mod_rate(rate);
			m_r_late_rev.set_diffusion_mod_rate(rate);
		} {
			uint32_t seed = static_cast<uint32_t>(params.late_diffusion_seed);
			m_l_late_rev.set_diffusion_seed(seed);
			m_r_late_rev.set_diffusion_seed(seed);
		}

		{ // Filters
			float cutoff = params.late_low_shelf_cutoff;
			m_l_late_rev.set_low_shelf_cutoff(cutoff);
			m_r_late_rev.set_low_shelf_cutoff(cutoff);
		} {
			float gain = dBtoGain(params.late_low_shelf_gain);
			m_l_late_rev.set_low_shelf_gain(gain);
			m_r_late_rev.set_low_shelf_gain(gain);
		} {
			float cutoff = params.late_high_shelf_cutoff;
			m_l_late_rev.set_high_shelf_cutoff(cutoff);
			m_r_late_rev.set_high_shelf_cutoff(cutoff);
		} {
			float gain = dBtoGain(params.late_high_shelf_gain);
			m_l_late_rev.set_high_shelf_gain(gain);
			m_r_late_rev.set_high_shelf_gain(gain);
		} {
			float cutoff = params.late_high_cut_cutoff;
			m_l_late_rev.set_high_cut_cutoff(cutoff);
			m_r_late_rev.set_high_cut_cutoff(cutoff);
		}
	}
}
