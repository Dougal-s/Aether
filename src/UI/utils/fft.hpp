#include <bit>
#include <cmath>
#include <complex>
#include <numbers>
#include <numeric>
#include <span>


namespace fft {
	/*
		applies the hann window function to the input
	*/
	template <class Container>
	void window_function(Container& container) {
		const auto coefm = std::exp(std::complex<float>(0, std::numbers::pi_v<float> / (container.size() - 1)));
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

	template <class Container>
	void bitReverseShuffle(Container& container) {
		assert(std::has_single_bit(container.size()));
		uint8_t numBits = std::countr_zero(container.size());

		for (size_t i = 0; i < container.size(); ++i) {
			size_t j = reverseBits(i, numBits);
			if (i < j) std::swap(container[i], container[j]);
		}
	}

	/*
		performs bit reverse fft inplace
		requires the input buffer size to be a power of 2
	*/
	template <class Container>
	void fft(Container& container) {
		assert(std::has_single_bit(container.size()));

		bitReverseShuffle(container);

		for (size_t m = 2; m <= container.size(); m <<= 1) {
			const auto wm = std::exp(std::complex<float>(0.f, -2.f * std::numbers::pi_v<float> / m));
			for (size_t k = 0; k < container.size(); k += m) {
				std::complex<float> w = 1;
				for (size_t j = 0; 2 * j < m; ++j, w *= wm) {
					const auto t = w * container[k + j + m / 2];
					const auto u = container[k + j];
					container[k + j] = u + t;
					container[k + j + m / 2] = u - t;
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
		assert(std::has_single_bit(container.size()));

		std::span<std::complex<float>> input(
			reinterpret_cast<std::complex<float>*>(container.data()),
			container.size()/2
		);

		fft(input);

		container[0] = input[0].imag() + input[0].real();

		const auto wm = std::exp(std::complex<float>(0.f, -2.f*std::numbers::pi_v<float> / container.size()));
		auto w = wm;
		for (size_t r = 1; r < input.size() / 2; ++r, w *= wm) {
			const auto i_r = input[r];
			const auto i_smr = input[input.size() - r];

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

		std::copy(container.end()-input.size()/2+1, container.end(), container.begin()+input.size()/2);
	}
}
