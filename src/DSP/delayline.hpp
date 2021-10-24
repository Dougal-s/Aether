#ifndef DELAYLINE_HPP
#define DELAYLINE_HPP

#include <array>
#include <cassert>
#include <cstdint>
#include <random>

#include "delay.hpp"
#include "diffuser.hpp"
#include "filters.hpp"

#include "utils/random.hpp"

#include "../common/constants.hpp"

class Delayline {
public:
	enum class Order { pre = 0, post = 1 };

	struct Filters {
		struct PushInfo {
			bool ls_enable;
			bool hs_enable;
			bool hc_enable;
		};

		Filters(double rate) : ls(rate), hs(rate), hc(rate) {}

		double push(double sample, PushInfo info) noexcept {
			if (info.ls_enable) sample = ls.push(sample);
			if (info.hs_enable) sample = hs.push(sample);
			if (info.hc_enable) sample = hc.push(sample);
			return sample;
		}

		void clear() {
			ls.clear();
			hs.clear();
			hc.clear();
		}

		Lowshelf<double> ls;
		Highshelf<double> hs;
		Lowpass6dB<double> hc;
	};

	struct PushInfo {
		Order order;
		AllpassDiffuser<double>::PushInfo diffuser_info;
		Filters::PushInfo damping_info;
	};

	ModulatedDelay<double> delay;
	AllpassDiffuser<double> diffuser;
	Filters damping;

	// Member Functions

	template <class RNG>
	Delayline(float rate, RNG& rng) :
		delay(rate, std::uniform_real_distribution<float>{0.f, 1.f}(rng) ),
		diffuser(rate, rng),
		damping(static_cast<double>(rate))
	{}

	void set_feedback(float feedback) { m_feedback = static_cast<double>(feedback); }

	double push(double sample, PushInfo info) {
		m_last_out = damping.push(m_last_out, info.damping_info);

		sample += m_last_out*m_feedback;

		assert(info.order == Order::pre || info.order == Order::post);
		switch(info.order) {
			case Order::pre:
				sample = delay.push(sample);
				m_last_out = diffuser.push(sample, info.diffuser_info);
				break;
			case Order::post:
				sample = diffuser.push(sample, info.diffuser_info);
				m_last_out = delay.push(sample);
				break;
		}

		return sample;
	}

	void clear() noexcept {
		m_last_out = 0;
		delay.clear();
		diffuser.clear();
		damping.clear();
	}

private:
	double m_last_out = 0;

	double m_feedback = 0;
};


class LateRev {
public:
	template <class RNG>
	LateRev(float rate, RNG& rng) : m_delay_lines{
		Delayline(rate, rng), Delayline(rate, rng), Delayline(rate, rng),
		Delayline(rate, rng), Delayline(rate, rng), Delayline(rate, rng),
		Delayline(rate, rng), Delayline(rate, rng), Delayline(rate, rng),
		Delayline(rate, rng), Delayline(rate, rng), Delayline(rate, rng)
	} {}

	// General
	void set_seed_crossmix(float crossmix) {
		m_crossmix = crossmix;

		Random::generate(m_rand, m_delay_seed, m_crossmix);
		generate_delay();
		generate_mod_depth();
		generate_mod_rate();

		for (auto& line : m_delay_lines)
			line.diffuser.set_seed_crossmix(crossmix);
	}

	void set_delay_lines(uint32_t lines) {
		if (m_lines < lines)
			for (uint32_t i = m_lines; i < lines; ++i)
				m_delay_lines[i].clear();
		m_lines = lines;
		m_gain_target = 0.3f+0.3f*max_lines/static_cast<float>(7+m_lines);
	}

	// delay line
	void set_delay(float delay) {
		m_gain_smoothing = std::exp(-2*constants::pi_v<float> / delay);
		m_delay = delay;
		generate_delay();
	}

	void set_delay_mod_depth(float mod_depth) {
		m_mod_depth = mod_depth;
		generate_mod_depth();
	}
	void set_delay_mod_rate(float mod_rate) {
		m_mod_rate = mod_rate;
		generate_mod_rate();
	}
	void set_delay_feedback(float feedback) {
		m_feedback = feedback;
		generate_feedback();
	}
	void set_delay_seed(uint32_t seed) {
		m_delay_seed = seed;

		Random::generate(m_rand, m_delay_seed, m_crossmix);
		generate_delay();
		generate_mod_depth();
		generate_mod_rate();
	}

	// diffusion
	void set_diffusion_drive(float drive) {
		for (auto& line : m_delay_lines)
			line.diffuser.set_drive(drive);
	}
	void set_diffusion_delay(float delay) {
		for (auto& line : m_delay_lines)
			line.diffuser.set_delay(delay);
	}
	void set_diffusion_mod_depth(float mod_depth) {
		for (auto& line : m_delay_lines)
			line.diffuser.set_mod_depth(mod_depth);
	}
	void set_diffusion_mod_rate(float mod_rate) {
		for (auto& line : m_delay_lines)
			line.diffuser.set_mod_rate(mod_rate);
	}
	void set_diffusion_seed(uint32_t seed) {
		uint32_t i = 1;
		for (auto& line : m_delay_lines)
			line.diffuser.set_seed(seed*i++);
	}

	// Filter
	void set_low_shelf_cutoff(float cutoff) {
		for (auto& line : m_delay_lines)
			line.damping.ls.set_cutoff(static_cast<double>(cutoff));
	}
	void set_low_shelf_gain(float gain) {
		for (auto& line : m_delay_lines)
			line.damping.ls.set_gain(static_cast<double>(gain));
	}
	void set_high_shelf_cutoff(float cutoff) {
		for (auto& line : m_delay_lines)
			line.damping.hs.set_cutoff(static_cast<double>(cutoff));
	}
	void set_high_shelf_gain(float gain) {
		for (auto& line : m_delay_lines)
			line.damping.hs.set_gain(static_cast<double>(gain));
	}
	void set_high_cut_cutoff(float cutoff) {
		for (auto& line : m_delay_lines)
			line.damping.hc.set_cutoff(static_cast<double>(cutoff));
	}


	float push(
		float sample,
		Delayline::PushInfo push_info
	) noexcept {
		double output = 0;
		for (uint32_t i = 0; i < m_lines; ++i)
			output += m_delay_lines[i].push(static_cast<double>(sample), push_info);

		m_gain = m_gain - m_gain_smoothing*(m_gain-m_gain_target);
		return m_gain*static_cast<float>(output);
	}

	static constexpr uint32_t max_lines = 12;

	static constexpr float max_delay = ModulatedDelay<double>::max_delay/1.5f;
	static constexpr float max_delay_mod = ModulatedDelay<double>::max_mod/1.15f;

	static constexpr float max_diffuse_delay_mod = ModulatedDelay<double>::max_mod/1.15f;
private:
	std::array<Delayline, max_lines> m_delay_lines;
	std::array<float, 3*max_lines> m_rand = {};

	// gain compensation for the number of delay lines
	float m_gain_target = 1.f;
	float m_gain_smoothing = 1.f;
	float m_gain = 1.f;

	uint32_t m_lines = 0;
	float m_delay = 0.f;
	float m_mod_depth = 0.f;
	float m_mod_rate = 0.f;
	float m_feedback = 0.f;

	uint32_t m_delay_seed = 0;
	float m_crossmix = 0.f;

	void generate_delay() {
		for (uint32_t line = 0; line < max_lines; ++line) {
			float delay = m_delay*(0.5f + 1.f*m_rand[line + 2*max_lines]);
			m_delay_lines[line].delay.set_delay(delay);
		}
	}

	void generate_mod_depth() {
		for (uint32_t line = 0; line < max_lines; ++line) {
			float mod_depth = m_mod_depth*(0.7f + 0.3f*m_rand[line]);
			m_delay_lines[line].delay.set_mod_depth(mod_depth);
		}
	}

	void generate_mod_rate() {
		for (uint32_t line = 0; line < max_lines; ++line) {
			float mod_rate = m_mod_rate*(0.7f + 0.3f*m_rand[line + max_lines]);
			m_delay_lines[line].delay.set_mod_rate(mod_rate);
		}
	}

	void generate_feedback() {
		for (uint32_t line = 0; line < max_lines; ++line) {
			float delay = m_delay*(0.5f + 1.f*m_rand[line + 2*max_lines]);
			// keep reverb time consistent between different lines
			float feedback = std::pow(m_feedback, delay/m_delay);
			m_delay_lines[line].set_feedback(feedback);
		}
	}
};


#endif
