#include <cassert>
#include <complex>
#include <cstddef>
#include <cstdint>

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

		// normalize so as not to reduce the volume
		for (float& e : container) e *= container.size() / sum;
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
		auto numBits = static_cast<uint8_t>(bits::countr_zero(size));

		for (size_t i = 0; i < size; ++i) {
			size_t j = reverseBits(i, numBits);
			if (i < j) std::swap(first[i], first[j]);
		}
	}

	/*
		performs bit reverse fft inplace
		requires the input buffer size to be a power of 2

		https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm#Data_reordering,_bit_reversal,_and_in-place_algorithms
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

		// Let x be the input of size N,
		// e be the even indexed elements of x,
		// o be the odd indexed elements of x,
		// and X, E, O be their respective DFTs

		// X can be reexpressed using the formula:
		// X[n] = E[n] + e^(2pi*i*n/N) * O[n]      [1]

		// Since e and o are both real sequences,
		// we can compute their DFTs using the
		// DFT of the sequence y = e + io, using:
		// E[n] = 1/2 * (Y[k] + Y*[N/2 - k])
		// O[n] = -i/2 * (Y[k] - Y*[N/2 - k])      [2]

		// [1]: https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm#The_radix-2_DIT_case
		// [2]: https://en.wikipedia.org/wiki/Discrete_Fourier_transform#Real_and_imaginary_part

		std::complex<float>* first = reinterpret_cast<std::complex<float>*>(container.data());
		const size_t size = container.size()/2;

		fft(first, first+size);

		container[0] = first[0].imag() + first[0].real();

		const auto wm = std::exp(std::complex<float>(0.f, -2.f*constants::pi_v<float> / container.size()));
		auto w = wm;
		for (size_t i = 1; i <= size/2; ++i) {
			const auto at_i = first[i];
			const auto at_smi = first[size-i];

			{
				const auto even = 0.5f * (at_i + std::conj(at_smi));
				const auto odd = std::complex<float>(0.f, -0.5f) * (at_i - std::conj(at_smi));

				container[i] = std::abs(even + w * odd) / container.size();
			}

			{
				const auto even = 0.5f * (at_smi + std::conj(at_i));
				const auto odd = std::complex<float>(0.f, -0.5f) * (at_smi - std::conj(at_i));

				container[container.size() - i] = std::abs(even - std::conj(w) * odd) / container.size();
			}

			w *= wm;
		}

		std::copy(container.end()-size/2, container.end(), container.begin()+size/2);
	}
}
