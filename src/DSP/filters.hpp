#ifndef FILTERS_HPP
#define FILTERS_HPP

#include <cmath>

#include "../common/constants.hpp"


// First Order Filters

/*
	RC Lowpass Filter

	f = cutoff frequency
	dt = 1 / samplerate

	a = 2pi*f*dt / (2pi*f*dt + 1)
	y[n] = a*x[n] + (1-a)*y[n-1]
*/
class Lowpass6dB {
public:
	Lowpass6dB(float rate, float cutoff = 0.f) :
		m_rate{rate},
		m_cutoff{cutoff},
		a{2.f*constants::pi_v<float>*cutoff/rate}
	{
		a = a/(a+1.f);
	}

	float push(float sample) noexcept {
		y = y + a*(sample-y);
		return y;
	}

	void clear() noexcept { y = 0.f; }

	void set_cutoff(float cutoff) noexcept {
		if (cutoff == m_cutoff)
			return;
		m_cutoff = cutoff;

		float w = 2.f*constants::pi_v<float>*m_cutoff/m_rate;
		a = w/(1+w);

		if (a == 0)
			y = 0;
	}

private:
	const float m_rate;
	float m_cutoff;
	float y = 0.f;
	float a;
};

/*
	Simple highpass filter
	calculated as:
	input - lowpassed
*/
class Highpass6dB {
public:
	Highpass6dB(float rate, float cutoff = 0.f) : m_lowpass(rate, cutoff) {}

	float push(float sample) noexcept {
		return sample - m_lowpass.push(sample);
	}

	void clear() noexcept { m_lowpass.clear(); }

	void set_cutoff(float cutoff) noexcept { m_lowpass.set_cutoff(cutoff); }

private:
	Lowpass6dB m_lowpass;
};


// Second Order Filters

/*
	A biquad filter implemented using transposed direct form 2
	More info can be found here:
	https://en.wikipedia.org/wiki/Digital_biquad_filter#Transposed_direct_forms

	Template class Generator is used to generate the filter coefficients
*/
template <class Generator>
class Biquad {
public:
	Biquad(
		float rate,
		float cutoff,
		float gain,
		std::tuple<float,float,float,float,float> coefs,
		Generator gen
	) :
		m_rate{rate},
		m_cutoff{cutoff},
		m_gain{gain},
		m_gen{gen},
		a1{std::get<0>(coefs)},
		a2{std::get<1>(coefs)},
		b0{std::get<2>(coefs)},
		b1{std::get<3>(coefs)},
		b2{std::get<4>(coefs)}
	{}

	Biquad(float rate, Generator gen = Generator{}) :
		Biquad(rate,0.f,1.f, gen(rate, 0.f,1.f), gen) {}

	void set_sample_rate(float rate) {
		m_rate = rate;
		std::tie(a1, a2, b0, b1, b2) = m_gen(m_rate, m_cutoff, m_gain);
	}

	void set_cutoff(float cutoff) {
		m_cutoff = cutoff;
		std::tie(a1, a2, b0, b1, b2) = m_gen(m_rate, m_cutoff, m_gain);
	}

	void set_gain(float gain) {
		m_gain = gain;
		std::tie(a1, a2, b0, b1, b2) = m_gen(m_rate, m_cutoff, m_gain);
	}

	float push(float x) noexcept {
		float y = b0*x + s1;
		s1 = s2 + b1*x - a1*y;
		s2 = b2*x - a2*y;
		return y;
	}

	void clear() noexcept { s1 = 0; s2 = 0; }
protected:
	float m_rate, m_cutoff, m_gain;
	// coefs
	[[no_unique_address]] Generator m_gen;
	float a1, a2, b0, b1, b2;
	// state
	float s1 = 0.f, s2 = 0.f;
};

struct LowshelfGenerator {
	constexpr auto operator()(float rate, float cutoff, float gain) noexcept {
		constexpr auto pi = constants::pi_v<float>;
		constexpr auto sqrt2 = constants::sqrt2_v<float>;

		float K = std::tan( pi*cutoff / rate );

		float a0 = 1 + sqrt2*K + K*K;

		float a1 = ( -2 + 2*K*K ) / a0;
		float a2 = ( 1 - sqrt2*K + K*K ) / a0;

		const float sqrt2G = std::sqrt(2*gain);
		float b0 = ( 1 + sqrt2G*K + gain*K*K) / a0;
		float b1 = (-2 + 2*gain*K*K )  / a0;
		float b2 = ( 1 - sqrt2G*K + gain*K*K) / a0;

		return std::make_tuple(a1, a2, b0, b1, b2);
	}
};

struct HighshelfGenerator {
	constexpr auto operator()(float rate, float cutoff, float gain) noexcept {
		constexpr auto pi = constants::pi_v<float>;
		constexpr auto sqrt2 = constants::sqrt2_v<float>;

		float K = std::tan( pi*cutoff / rate );

		const float sqrt2G = std::sqrt(2*gain);
		float a0 = 1 + sqrt2G*K + gain*K*K;

		float a1 = (-2 + 2*gain*K*K )  / a0;
		float a2 = ( 1 - sqrt2G*K + gain*K*K) / a0;

		float b0 = gain*(1 + sqrt2*K + K*K) / a0;
		float b1 = gain*( -2 + 2*K*K ) / a0;
		float b2 = gain*( 1 - sqrt2*K + K*K ) / a0;

		return std::make_tuple(a1, a2, b0, b1, b2);
	}
};

using Lowshelf = Biquad<LowshelfGenerator>;
using Highshelf = Biquad<HighshelfGenerator>;

#endif
