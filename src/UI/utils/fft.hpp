#include <cmath>
#include <complex>
#include <numeric>

#include "../../common/constants.hpp"
#include "../../common/bit_ops.hpp"

namespace fft {
	/*
		applies the hann window function to the input
	*/
	template <class Container>
	void window_function(Container& container) {
		const auto coefm = std::exp(std::complex<float>(0, constants::pi_v<float> / (container.size() - 1)));
		std::complex<float> coef = {1, 0};
		float sum = 0.f;
		for (float& e : container) {
			coef *= coefm;
			e *= coef.imag()*coef.imag();
			sum += coef.imag()*coef.imag();
		}
		for (float& e : container) e /= sum;
	}

	/*
		Reverses the n least significant bits in val
	*/
	constexpr static size_t reverseBits(size_t val, uint8_t n) {
		size_t reversed = 0;
		for (uint8_t i = 0; i < n; ++i) {
			reversed <<= 1;
			reversed |= val & 1;
			val >>= 1;
		}
		return reversed;
	}

	template <class Iterator>
	void bitReverseShuffle(Iterator first, Iterator last) {
		const size_t size = last-first;
		assert(bits::has_single_bit(size));
		uint8_t numBits = bits::countr_zero(size);

		for (size_t i = 0; i < size; ++i) {
			size_t j = reverseBits(i, numBits);
			if (i < j) std::swap(first[i], first[j]);
		}
	}

	/*
		performs bit reverse fft inplace
		requires the input buffer size to be a power of 2
	*/
	template <class Iterator>
	void fft(Iterator first, Iterator last) {
		const size_t size = last-first;
		assert(bits::has_single_bit(size));

		bitReverseShuffle(first, last);

		for (size_t m = 2; m <= size; m <<= 1) {
			const auto wm = std::exp(std::complex<float>(0.f, -2.f * constants::pi_v<float> / m));
			for (size_t k = 0; k < size; k += m) {
				std::complex<float> w = 1;
				for (size_t j = 0; 2 * j < m; ++j, w *= wm) {
					const auto t = w * first[k + j + m / 2];
					const auto u = first[k + j];
					first[k + j] = u + t;
					first[k + j + m / 2] = u - t;
				}
			}
		}
	}

	/*
		computes the magnitude of each frequency inplace
		requires an input size that is a power of two
	*/
	template <class Container>
	void magnitudes(Container& container) {
		assert(bits::has_single_bit(container.size()));

		std::complex<float>* first = reinterpret_cast<std::complex<float>*>(container.data());
		const size_t size = container.size()/2;

		fft(first, first+size);

		container[0] = first[0].imag() + first[0].real();

		const auto wm = std::exp(std::complex<float>(0.f, -2.f*constants::pi_v<float> / container.size()));
		auto w = wm;
		for (size_t r = 1; r < size / 2; ++r, w *= wm) {
			const auto i_r = first[r];
			const auto i_smr = first[size - r];

			{
				const auto F = 0.5f * (i_r + std::conj(i_smr));
				const auto G = std::complex<float>(0, 0.5f) * (std::conj(i_smr) - i_r);

				container[r] = 2.f*std::abs(F + w * G);
			}

			{
				const auto F = 0.5f * (i_smr + std::conj(i_r));
				const auto G = std::complex<float>(0, 0.5f) * (std::conj(i_r) - i_smr);

				container[container.size() - r] = 2.f*std::abs(F + w * G);
			}
		}

		std::copy(container.end()-size/2+1, container.end(), container.begin()+size/2);
	}
}
