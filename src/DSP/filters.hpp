#ifndef FILTERS_HPP
#define FILTERS_HPP

#include <cmath>
#include <tuple>

#include "../common/constants.hpp"


// First Order Filters

/*
	RC Lowpass Filter

	f = cutoff frequency
	dt = 1 / samplerate

	a = 2pi*f*dt / (2pi*f*dt + 1)
	y[n] = a*x[n] + (1-a)*y[n-1]
*/
template <class FpType>
class Lowpass6dB {
public:
	Lowpass6dB(FpType rate, FpType cutoff = 0) : m_rate{rate}, a{} {
		set_cutoff(cutoff);
	}

	FpType push(FpType sample) noexcept {
		y = y + a*(sample-y);
		return y;
	}

	void clear() noexcept { y = 0; }

	void set_cutoff(FpType cutoff) noexcept {
		FpType w = 2*constants::pi_v<FpType>*cutoff/m_rate;
		a = w/(1+w);

		if (a == 0)
			y = 0;
	}

private:
	const FpType m_rate;
	FpType y = 0;
	FpType a;
};

/*
	Simple highpass filter
	calculated as:
	input - lowpassed
*/
template <class FpType>
class Highpass6dB {
public:
	Highpass6dB(FpType rate, FpType cutoff = 0) : m_lowpass(rate, cutoff) {}

	FpType push(FpType sample) noexcept {
		return sample - m_lowpass.push(sample);
	}

	void clear() noexcept { m_lowpass.clear(); }

	void set_cutoff(FpType cutoff) noexcept { m_lowpass.set_cutoff(cutoff); }

private:
	Lowpass6dB<FpType> m_lowpass;
};


// Second Order Filters

/*
	A biquad filter implemented using transposed direct form 2
	More info can be found here:
	https://en.wikipedia.org/wiki/Digital_biquad_filter#Transposed_direct_forms

	Template class Generator is used to generate the filter coefficients
*/
template <class Generator, class FpType>
class Biquad {
public:
	Biquad(
		FpType rate,
		FpType cutoff,
		FpType gain,
		std::tuple<FpType,FpType,FpType,FpType,FpType> coefs,
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

	Biquad(FpType rate, Generator gen = Generator{}) :
		Biquad(rate,0,1, gen(rate, static_cast<FpType>(0),static_cast<FpType>(1)), gen) {}

	void set_sample_rate(FpType rate) {
		m_rate = rate;
		std::tie(a1, a2, b0, b1, b2) = m_gen(m_rate, m_cutoff, m_gain);
	}

	void set_cutoff(FpType cutoff) {
		m_cutoff = cutoff;
		std::tie(a1, a2, b0, b1, b2) = m_gen(m_rate, m_cutoff, m_gain);
	}

	void set_gain(FpType gain) {
		m_gain = gain;
		std::tie(a1, a2, b0, b1, b2) = m_gen(m_rate, m_cutoff, m_gain);
	}

	FpType push(FpType x) noexcept {
		FpType y = b0*x + s1;
		s1 = s2 + b1*x - a1*y;
		s2 = b2*x - a2*y;
		return y;
	}

	void clear() noexcept { s1 = 0; s2 = 0; }
protected:
	FpType m_rate, m_cutoff, m_gain;
	// coefs
	[[no_unique_address]] Generator m_gen;
	FpType a1, a2, b0, b1, b2;
	// state
	FpType s1 = 0, s2 = 0;
};

struct LowshelfGenerator {
	template <class FpType>
	auto operator()(FpType rate, FpType cutoff, FpType gain) noexcept {
		constexpr auto pi = constants::pi_v<FpType>;
		constexpr auto sqrt2 = constants::sqrt2_v<FpType>;

		FpType K = std::tan( pi*cutoff / rate );

		FpType a0 = 1 + sqrt2*K + K*K;

		FpType a1 = ( -2 + 2*K*K ) / a0;
		FpType a2 = ( 1 - sqrt2*K + K*K ) / a0;

		const FpType sqrt2G = std::sqrt(2*gain);
		FpType b0 = ( 1 + sqrt2G*K + gain*K*K) / a0;
		FpType b1 = (-2 + 2*gain*K*K )  / a0;
		FpType b2 = ( 1 - sqrt2G*K + gain*K*K) / a0;

		return std::make_tuple(a1, a2, b0, b1, b2);
	}
};

struct HighshelfGenerator {
	template <class FpType>
	auto operator()(FpType rate, FpType cutoff, FpType gain) noexcept {
		constexpr auto pi = constants::pi_v<FpType>;
		constexpr auto sqrt2 = constants::sqrt2_v<FpType>;

		FpType K = std::tan( pi*cutoff / rate );

		const FpType sqrt2G = std::sqrt(2*gain);
		FpType a0 = 1 + sqrt2G*K + gain*K*K;

		FpType a1 = (-2 + 2*gain*K*K )  / a0;
		FpType a2 = ( 1 - sqrt2G*K + gain*K*K) / a0;

		FpType b0 = gain*(1 + sqrt2*K + K*K) / a0;
		FpType b1 = gain*( -2 + 2*K*K ) / a0;
		FpType b2 = gain*( 1 - sqrt2*K + K*K ) / a0;

		return std::make_tuple(a1, a2, b0, b1, b2);
	}
};

template <class FpType> using Lowshelf = Biquad<LowshelfGenerator, FpType>;
template <class FpType> using Highshelf = Biquad<HighshelfGenerator, FpType>;

#endif
