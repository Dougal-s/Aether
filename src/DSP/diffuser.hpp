#ifndef DIFFUSER_HPP
#define DIFFUSER_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <random>
#include <utility>

#include "utils/random.hpp"
#include "utils/ringbuffer.hpp"
#include "utils/lfo.hpp"

/*
	Schroeder Allpass filter
*/
class ModulatedAllpass {
public:
	ModulatedAllpass() = default;
	ModulatedAllpass(float rate, float mod_phase);
	ModulatedAllpass(ModulatedAllpass&& other);
	ModulatedAllpass(const ModulatedAllpass&) = delete;

	ModulatedAllpass& operator=(ModulatedAllpass&& other) noexcept;
	ModulatedAllpass& operator=(const ModulatedAllpass&) = delete;

	void set_delay(float delay) noexcept {
		assert(delay >= 1.f);
		m_delay = delay;
		m_mod_depth = std::min(m_mod_depth, delay-1.f);
	}

	void set_mod_depth(float mod_depth) noexcept {
		m_mod_depth = std::min(mod_depth, m_delay-1.f);
	}

	void set_mod_rate(float mod_rate) noexcept { m_lfo.set_rate(mod_rate); }

	float push(float sample, bool interpolate, float feedback) noexcept;

	void clear() noexcept { m_buf.clear(); }

	// [10ms, 100ms]
	static constexpr std::pair<float, float> delay_bounds = {0.01f, 0.1f};
	// [0ms, 3ms]
	static constexpr std::pair<float, float> mod_bounds = {0.f, 0.003f};

private:
	Ringbuffer<float> m_buf = {};

	float m_delay = 1.f;
	float m_mod_depth = 0.f;

	LFO m_lfo = {};
};


inline ModulatedAllpass::ModulatedAllpass(float rate, float mod_phase) :
	m_buf{static_cast<size_t>((delay_bounds.second+mod_bounds.second) * rate)},
	m_lfo{mod_phase} {}

inline ModulatedAllpass::ModulatedAllpass(ModulatedAllpass&& other) :
	ModulatedAllpass()
{
	*this = std::move(other);
}

inline ModulatedAllpass& ModulatedAllpass::operator=(
	ModulatedAllpass&& other
) noexcept {
	std::swap(m_buf, other.m_buf);
	std::swap(m_delay, other.m_delay);
	std::swap(m_delay, other.m_mod_depth);
	std::swap(m_lfo, other.m_lfo);
	return *this;
}

inline float ModulatedAllpass::push(
	float sample,
	bool interpolate,
	float feedback
) noexcept {
	assert(static_cast<size_t>(m_delay + m_mod_depth) <= m_buf.size);
	assert(m_delay - m_mod_depth >= 1.f);

	float delay = m_delay + m_mod_depth*m_lfo.depth() - 1.f;
	m_lfo.next();

	uint32_t delay_floor = static_cast<uint32_t>(delay);
	size_t idx1 = m_buf.end - delay_floor + (m_buf.end < delay_floor ? m_buf.size : 0);
	size_t idx2 = idx1-1 + (idx1 < 1 ? m_buf.size : 0);
	float t = delay-static_cast<float>(delay_floor);
	float delayed = interpolate ?
		std::lerp(m_buf.buf[idx1], m_buf.buf[idx2], t) : m_buf.buf[idx1];

	m_buf.push(sample + delayed*feedback);

	return delayed - m_buf.buf[m_buf.end]*feedback;
}



/*
	An allpass diffuser consisting of up
	to 8 modulated allpass filters in series
*/
class AllpassDiffuser {
public:
	struct PushInfo {
		uint32_t stages;
		float feedback;
		bool interpolate;
	};

	template <class RNG>
	AllpassDiffuser(float sample_rate, RNG& rng);
	AllpassDiffuser(const AllpassDiffuser&) = delete;
	~AllpassDiffuser() = default;

	AllpassDiffuser& operator=(const AllpassDiffuser&) = delete;

	void set_seed(uint32_t seed) noexcept;
	void set_seed_crossmix(float crossmix) noexcept;
	void set_delay(float delay) noexcept;
	void set_mod_depth(float mod_depth) noexcept;
	void set_mod_rate(float mod_rate) noexcept;

	float push(float sample, PushInfo info) noexcept {
		for (uint32_t i = 0; i < info.stages; ++i)
			sample = m_filters[i].push(sample, info.interpolate, info.feedback);
		return sample;
	}

	void clear() noexcept {
		for (auto& filter : m_filters)
			filter.clear();
	}

	static constexpr uint32_t max_stages = 8;

	static constexpr std::pair<float, float> delay_bounds = ModulatedAllpass::delay_bounds;
	static constexpr std::pair<float, float> mod_bounds = {
		ModulatedAllpass::mod_bounds.first/0.85f,
		ModulatedAllpass::mod_bounds.second/1.15f
	};
private:
	std::array<ModulatedAllpass, max_stages> m_filters = {};
	// used for mod_amt, mod_rate and delay
	std::array<float, 3*max_stages> m_rand_vals = {};

	float m_delay = 10.f;
	float m_mod_depth = 0.f;
	float m_mod_rate = 0.f;

	uint32_t m_seed = 0;
	float m_crossmix = 0.f;

	float m_rate;

	void generate_delay() noexcept;
	void generate_mod_depth() noexcept;
	void generate_mod_rate() noexcept;
};


template <class RNG>
AllpassDiffuser::AllpassDiffuser(float rate, RNG& rng) : m_rate(rate) {
	std::uniform_real_distribution<float> dist(0.f, 1.f);
	for (auto& filter : m_filters)
		filter = ModulatedAllpass(rate, dist(rng));

	Random::generate(m_rand_vals, m_seed, m_crossmix);
}


inline void AllpassDiffuser::set_seed(uint32_t seed) noexcept {
	if (m_seed == seed) return;
	m_seed = seed;

	Random::generate(m_rand_vals, m_seed, m_crossmix);
	generate_delay();
	generate_mod_rate();
}

inline void AllpassDiffuser::set_seed_crossmix(float crossmix) noexcept {
	if (m_crossmix == crossmix) return;
	m_crossmix = crossmix;

	Random::generate(m_rand_vals, m_seed, m_crossmix);
	generate_delay();
	generate_mod_rate();
}

inline void AllpassDiffuser::set_delay(float delay) noexcept {
	if (m_delay == delay) return;
	m_delay = delay;

	generate_delay();
}

inline void AllpassDiffuser::set_mod_depth(float mod_depth) noexcept {
	if (m_mod_depth == mod_depth) return;
	m_mod_depth = mod_depth;

	generate_mod_depth();
}

inline void AllpassDiffuser::set_mod_rate(float mod_rate) noexcept {
	if (m_mod_rate == mod_rate) return;
	m_mod_rate = mod_rate;

	generate_mod_rate();
}

inline void AllpassDiffuser::generate_delay() noexcept {
	for (size_t filter = 0; filter < m_filters.size(); ++filter) {
		m_filters[filter].set_delay(
			m_delay*std::exp( -2.3f*m_rand_vals[filter] )
		);
	}
}

inline void AllpassDiffuser::generate_mod_depth() noexcept {
	for (size_t filter = 0; filter < m_filters.size(); ++filter) {
		m_filters[filter].set_mod_depth(
			m_mod_depth * (0.85f + 0.3f*m_rand_vals[max_stages+filter])
		);
	}
}

inline void AllpassDiffuser::generate_mod_rate() noexcept {
	for (size_t filter = 0; filter < m_filters.size(); ++filter) {
		m_filters[filter].set_mod_rate(
			m_mod_rate * (0.85f + 0.3f*m_rand_vals[2*max_stages+filter]) / m_rate
		);
	}
}


#endif
