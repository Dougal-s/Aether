#ifndef BIT_OPS_HPP
#define BIT_OPS_HPP

#if __has_include (<version>)
	#include <version>
#endif

#if __has_include (<bit>)
	#include <bit>
#endif

namespace bits {

#if __cpp_lib_int_pow2 == 202002L
	template <class T>
	constexpr bool has_single_bit(T x) {
		return std::has_single_bit(static_cast<std::make_unsigned_t<T>>(x));
	}

	template <class T>
	constexpr T bit_ceil(T x) {
		return std::bit_ceil(static_cast<std::make_unsigned_t<T>>(x));
	}
#else
	/*
		from http://www.graphics.stanford.edu/~seander/bithacks.html
	*/
	template <class T>
	constexpr bool has_single_bit(T x) {
		return x && !(x & (x - 1));
	}

	template <class T>
	constexpr T bit_ceil(T x) {
		T y = 1;
		while (y < x) y <<= 1;
		return y;
	}

#endif


#if __cpp_lib_bitops == 201907L
	template <class T>
	constexpr int countr_zero(T x) {
		return std::countr_zero(static_cast<std::make_unsigned_t<T>>(x));
	}
#else
	template <class T>
	constexpr int countr_zero(T x) {
		int count = 0;
		while (1 ^ (x & 1)) {
			x >>= 1;
			++count;
		}
		return count;
	}
#endif
}

#endif
