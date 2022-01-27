#ifndef AETHER_MATH_HPP
#define AETHER_MATH_HPP

#include <cmath>

#if __has_include (<version>)
	#include <version>
#endif

namespace math {
	#if __cpp_lib_interpolate >= 201902L
	template <class T>
	inline constexpr T lerp(T a, T b, T t) noexcept { return std::lerp(a, b, t); }
	#else
	template <class T>
	inline constexpr T lerp(T a, T b, T t) noexcept { return a + t*(b-a); }
	#endif
}

#endif
