#include <cfenv>
#if !defined(FE_DFL_DISABLE_SSE_DENORMS_ENV) && __has_include(<xmmintrin.h>)
	#include <xmmintrin.h>
#endif

void truncate_denormals_to_zero() noexcept {
	#ifdef FE_DFL_DISABLE_SSE_DENORMS_ENV
		std::fesetenv(FE_DFL_DISABLE_SSE_DENORMS_ENV);
	#elif __has_include(<xmmintrin.h>)
		_mm_setcsr (_mm_getcsr() | 0x8040);
	#endif
}
