#ifndef DELAY_HPP
#define DELAY_HPP

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>

#include "utils/lfo.hpp"
#include "utils/random.hpp"
#include "utils/ringbuffer.hpp"

/*
	A basic tap delay
*/
class Delay {
public:
	explicit Delay(float rate) : m_buf{static_cast<size_t>(max_delay*rate)+1} {}
	Delay(const Delay&) = delete;

	Delay& operator=(const Delay&) = delete;

	float push(float sample, size_t delay) noexcept {
		assert(delay < m_buf.size);

		m_buf.push(sample);

		auto idx = m_buf.end - delay + (m_buf.end < delay ? m_buf.size: 0);
		return m_buf.buf[idx];
	}

	void clear() noexcept { m_buf.clear(); }

	// maximum delay in seconds
	static constexpr float max_delay = 0.5f;

private:
	Ringbuffer<float> m_buf;
};


/*
	A tap delay with a modulated delay length
*/
template <class FpType>
class ModulatedDelay {
public:
	ModulatedDelay(float sample_rate, float phase) :
		m_buf{static_cast<size_t>( (max_delay+max_mod) * sample_rate ) + 1},
		m_lfo(phase) {}
	ModulatedDelay(const ModulatedDelay&) = delete;

	ModulatedDelay& operator=(const ModulatedDelay&) = delete;

	void set_delay(float delay) noexcept {
		assert(static_cast<size_t>(m_mod_depth + m_delay) < m_buf.size);
		m_delay = delay;
	}
	void set_mod_depth(float mod_depth) noexcept {
		assert(static_cast<size_t>(m_mod_depth + m_delay) < m_buf.size);
		m_mod_depth = mod_depth;
	}
	void set_mod_rate(float mod_rate) noexcept { m_lfo.set_rate(mod_rate); }

	void clear() noexcept { m_buf.clear(); }

	FpType push(FpType sample) noexcept {
		m_buf.push(sample);

		float delay = std::max(m_delay + m_mod_depth*m_lfo.depth(), 0.f);
		m_lfo.next();

		uint32_t delay_floor = static_cast<uint32_t>(delay);
		FpType t = static_cast<FpType>(delay - static_cast<float>(delay_floor));

		size_t idx1 = m_buf.end - delay_floor
			+ (m_buf.end < delay_floor ? m_buf.size : 0);
		size_t idx2 = idx1 - 1 + (idx1 < 1 ? m_buf.size : 0);

		return m_buf.buf[idx1] + t*(m_buf.buf[idx2]-m_buf.buf[idx1]);
	}

	// maximum in seconds
	static constexpr float max_delay = 1.5f;
	static constexpr float max_mod = 0.05f;

private:
	Ringbuffer<FpType> m_buf;
	LFO m_lfo;

	float m_delay = 0.f;
	float m_mod_depth = 0.f;
};


/*
	A single delaybuffer with multiple delay taps
*/
class MultitapDelay {
public:

	MultitapDelay(float rate);
	MultitapDelay(const MultitapDelay&) = delete;

	MultitapDelay& operator=(const MultitapDelay&) = delete;

	void set_seed(uint32_t seed) noexcept;
	void set_seed_crossmix(float crossmix) noexcept;
	void set_decay(float decay) noexcept;

	float push(float sample, uint32_t taps, float length);

	void clear() noexcept { m_buf.clear(); }

	static constexpr uint32_t max_taps = 50;
	static constexpr float max_length = 0.5f;
private:
	Ringbuffer<float> m_buf;

	std::array<float, max_taps> m_tap_gain = {};
	std::array<float, max_taps> m_tap_delay = {};

	std::array<float, 2*max_taps> m_rand_vals = {};

	float m_decay = 0.5f;
	uint32_t m_seed = 0;
	float m_crossmix = 0.5f;

	void generate_tap_delays() noexcept;
	void generate_tap_gains() noexcept;
};


inline MultitapDelay::MultitapDelay(float rate) :
	m_buf{static_cast<size_t>(max_length*rate) + 1}
{
	Random::generate(m_rand_vals, m_seed, m_crossmix);
	generate_tap_delays();
	generate_tap_gains();
}

inline float MultitapDelay::push(float sample, uint32_t taps, float length) {
	assert(static_cast<size_t>(length) < m_buf.size);
	assert(taps <= max_taps);

	m_buf.push(sample);

	const float delay_coef = length/m_tap_delay[taps-1];
	float output = 0.f;
	for (uint32_t i = 0; i < taps; ++i) {
		uint32_t delay = static_cast<uint32_t>(m_tap_delay[i]*delay_coef);
		size_t idx = m_buf.end - delay + (m_buf.end < delay ? m_buf.size : 0);
		output += m_tap_gain[i]*m_buf.buf[idx];
	}

	// adjust the loudness depending on the number of taps
	const float adjust = 0.35f+0.21f*max_taps/static_cast<float>(20+taps);
	return output*adjust;
}

inline void MultitapDelay::set_seed(uint32_t seed) noexcept {
	m_seed = seed;

	Random::generate(m_rand_vals, m_seed, m_crossmix);
	generate_tap_delays();
	generate_tap_gains();
}

inline void MultitapDelay::set_seed_crossmix(float crossmix) noexcept {
	m_crossmix = crossmix;

	Random::generate(m_rand_vals, m_seed, m_crossmix);
	generate_tap_delays();
	generate_tap_gains();
}

inline void MultitapDelay::set_decay(float decay) noexcept {
	m_decay = decay;

	generate_tap_gains();
}


inline void MultitapDelay::generate_tap_delays() noexcept {
	std::partial_sum(
		m_rand_vals.begin(), m_rand_vals.begin()+max_taps,
		m_tap_delay.begin()
	);
}

inline void MultitapDelay::generate_tap_gains() noexcept {
	for (size_t tap = 0; tap < m_tap_gain.size(); ++tap) {
		float gain = std::exp(-4.f*m_decay*m_tap_delay[tap] / (m_tap_delay.back()+1.f));
		m_tap_gain[tap] = gain*m_rand_vals[max_taps+tap];
	}
}

#endif
