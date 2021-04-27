#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

namespace constants {
	template <typename T>
	inline constexpr T sqrt2_v = T(1.414213562373095048801688724209698079l);
	inline constexpr double sqrt2 = sqrt2_v<double>;

	template <typename T>
	inline constexpr T pi_v = T(3.141592653589793238462643383279502884l);
	inline constexpr double pi = pi_v<double>;
}

#endif
