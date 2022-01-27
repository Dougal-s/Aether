#ifndef RANDOM_HPP
#define RANDOM_HPP

#include <cmath>
#include <cstdint>
#include <limits>
#include "math.hpp"

namespace Random {

	/*
		Xorshift64* pseudo random generator
	*/
	template <uint_fast8_t a, uint_fast8_t b, uint_fast8_t c, auto mult>
	class Xorshift64sEngine {
	public:
		using result_type = uint32_t;

		constexpr explicit Xorshift64sEngine(uint32_t seed) :
			m_state((seed << 1) + 1) {}

		constexpr void seed(uint32_t value) noexcept {
			m_state = (value << 1) + 1;
		}

		result_type operator()() noexcept {
			m_state ^= m_state >> a;
			m_state ^= m_state << b;
			m_state ^= m_state >> c;
			return static_cast<result_type>((m_state * mult) >> 32);
		}

		static constexpr result_type min() noexcept {
			return std::numeric_limits<result_type>::min();
		}
		static constexpr result_type max() noexcept {
			return std::numeric_limits<result_type>::max();
		}

	private:
		uint64_t m_state;
	};

	using Xorshift64s = Xorshift64sEngine<12, 25, 27, 0x2545F4914F6CDD1Du>;


	/*
		Fills the container with random values in the range [0.f, 1.f]
		generated from the seed 'seed'

		Note: because the function is interpolating between two uniform ranges,
		the output distribution will be more peak like at crossmix=0.5 and
		uniform at crossmix=0 and 1.
	*/
	template <class Container>
	void generate(Container& container, uint32_t seed, float cross_seed) noexcept {
		Xorshift64s rng1(seed);
		Xorshift64s rng2(~seed);
		for (auto& val : container) {
			val = math::lerp(
				static_cast<float>(rng1() >> 8) * 0x1.0p-24f,
				static_cast<float>(rng2() >> 8) * 0x1.0p-24f,
				cross_seed
			);
		}
	}
}

#endif
