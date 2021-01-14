#ifndef DELAYLINE_HPP
#define DELAYLINE_HPP

#include "delay.hpp"
#include "filters.hpp"

class Delayline {
public:
	enum class Order { pre = 0, post = 1 };

	struct Filters {
		struct PushInfo {
			bool ls_enable;
			bool hs_enable;
			bool hc_enable;
		};

		Filters(float rate) : ls(rate), hs(rate), hc(rate) {}

		float push(float sample, PushInfo info) noexcept {
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

		Lowshelf ls;
		Highshelf hs;
		Lowpass6dB hc;
	};

	struct PushInfo {
		Order order;
		AllpassDiffuser::PushInfo diffuser_info;
		Filters::PushInfo damping_info;
	};

	ModulatedDelay delay;
	AllpassDiffuser diffuser;
	Filters damping;

	// Member Functions

	template <class RNG>
	Delayline(float rate, RNG& rng) :
		delay(rate, std::uniform_real_distribution<float>{0.f, 1.f}(rng) ),
		diffuser(rate, rng),
		damping(rate)
	{}

	void set_feedback(float feedback) { m_feedback = feedback; }

	float push(float sample, PushInfo info) {
		m_last_out = damping.push(m_last_out, info.damping_info);

		sample += m_last_out*m_feedback;

		assert(info.order == Order::pre || order == Order::post);
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
		m_last_out = 0.f;
		delay.clear();
		diffuser.clear();
		damping.clear();
	}

private:
	float m_last_out = 0.f;

	float m_feedback = 0.f;
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
		if (m_crossmix == crossmix) return;
		m_crossmix = crossmix;

		Random::generate(m_rand, m_delay_seed, m_crossmix);
		generate_delay();
		generate_mod_depth();
		generate_mod_rate();

		for (auto& line : m_delay_lines)
			line.diffuser.set_seed_crossmix(crossmix);
	}

	// delay line
	void set_delay(float delay) {
		if (m_delay == delay) return;
		m_delay = delay;
		generate_delay();
	}

	void set_delay_mod_depth(float mod_depth) {
		if (m_mod_depth == mod_depth) return;
		m_mod_depth = mod_depth;
		generate_mod_depth();
	}
	void set_delay_mod_rate(float mod_rate) {
		if (m_mod_rate == mod_rate) return;
		m_mod_rate = mod_rate;
		generate_mod_rate();
	}
	void set_delay_feedback(float feedback) {
		if (m_feedback == feedback) return;
		m_feedback = feedback;
		generate_feedback();
	}
	void set_delay_seed(uint32_t seed) {
		if (m_delay_seed == seed) return;
		m_delay_seed = seed;

		Random::generate(m_rand, m_delay_seed, m_crossmix);
		generate_delay();
		generate_mod_depth();
		generate_mod_rate();
	}

	// diffusion
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
			line.damping.ls.set_cutoff(cutoff);
	}
	void set_low_shelf_gain(float gain) {
		for (auto& line : m_delay_lines)
			line.damping.ls.set_gain(gain);
	}
	void set_high_shelf_cutoff(float cutoff) {
		for (auto& line : m_delay_lines)
			line.damping.hs.set_cutoff(cutoff);
	}
	void set_high_shelf_gain(float gain) {
		for (auto& line : m_delay_lines)
			line.damping.hs.set_gain(gain);
	}
	void set_high_cut_cutoff(float cutoff) {
		for (auto& line : m_delay_lines)
			line.damping.hc.set_cutoff(cutoff);
	}


	float push(
		float sample,
		uint32_t lines,
		Delayline::PushInfo push_info
	) noexcept {
		float output = 0.f;
		for (uint32_t i = 0; i < lines; ++i)
			output += m_delay_lines[i].push(sample, push_info);

		return output * (0.3f+0.3f*max_lines/static_cast<float>(7+lines));
	}

	static constexpr uint32_t max_lines = 12;

	static constexpr float max_delay = ModulatedDelay::max_delay/1.5f;
	static constexpr float max_delay_mod = ModulatedDelay::max_mod/1.15f;

	static constexpr float max_diffuse_delay_mod = ModulatedDelay::max_mod/1.15f;
private:
	std::array<Delayline, max_lines> m_delay_lines;
	std::array<float, 3*max_lines> m_rand = {};

	float m_delay = 0.f;
	float m_mod_depth = 0.f;
	float m_mod_rate = 0.f;
	float m_feedback = 0.f;

	uint32_t m_delay_seed = 0.f;
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
