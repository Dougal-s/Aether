#ifndef LFO_HPP
#define LFO_HPP

#include <complex>

#include "../../common/constants.hpp"

class LFO {
	static constexpr double pi = constants::pi;
public:
	LFO() {}
	LFO(float phase, float rate = 0.f) : m_phase{std::polar(1.0, 2*pi*static_cast<double>(phase))} {
		set_rate(rate);
	}

	float depth() const noexcept { return static_cast<float>(m_phase.imag()); }

	void next() noexcept { m_phase *= m_step; }

	// rate is in cycles/sample
	void set_rate(float rate) noexcept {
		m_step = std::polar(1.0, 2*pi*static_cast<double>(rate));
	}

private:
	std::complex<double> m_step = 1.0;
	std::complex<double> m_phase = 1.0;
};

#endif
