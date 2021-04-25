#ifndef BIT_OPS_HPP
#define BIT_OPS_HPP

#include <version>

#ifdef __cpp_bit_ops

#if __has_include(<bit>)
	#include <bit>
#endif

namespace bits {
	constexpr auto has_single_bit = std::has_single_bit;
	constexpr auto bit_ceil = std::bit_ceil;
	constexpr auto countr_zero = std::countr_zero;
}

#else

namespace bits {

	/*
		from http://www.graphics.stanford.edu/~seander/bithacks.html
	*/
	template <class T>
	bool has_single_bit(T x) {
		return x && !(x & (x - 1));
	}

	template <class T>
	T bit_ceil(T x) {
		if (has_single_bit(x)) return x;
		unsigned lead = 0;
		while (x) {
			++lead;
			x >>= 1;
		}
		return 1 << lead;
	}

	template <class T>
	T countr_zero(T x) {
		unsigned count = 0;
		while (1 ^ (x & 1)) {
			x >>= 1;
			++count;
		}
		return count;
	}
}

#endif

#endif
