add_library(aether_dsp MODULE
	aether_dsp_lv2.cpp
	aether_dsp.cpp
	aether_dsp.hpp
	delay.hpp
	delayline.hpp
	diffuser.hpp
	filters.hpp
	utils/lfo.hpp
	utils/random.hpp
	utils/ringbuffer.hpp
)

target_compile_features(aether_dsp PUBLIC cxx_std_17)

set_target_properties(aether_dsp
	PROPERTIES
	CXX_VISIBILITY_PRESET hidden
	INTERPROCEDURAL_OPTIMIZATION TRUE
	PREFIX ""
)

# Compile Options

target_compile_definitions(aether_dsp PRIVATE "$<$<CONFIG:RELEASE>:NDEBUG>")

# Architecture
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)|(i386)|(i686)|(AMD64)")
	if (MSVC)
		target_compile_options(aether_dsp PRIVATE /arch:SSE2)
	else()
		target_compile_options(aether_dsp PRIVATE -msse2)
	endif()
endif()


# Platform

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_definitions(aether_dsp PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (MSVC)
	target_compile_options(aether_dsp PRIVATE
		"$<$<CONFIG:DEBUG>:/W4>"
		"$<$<CONFIG:RELEASE>:/O2>"
	)
else()
	target_compile_options(aether_dsp PRIVATE
		-Wall -Wextra -Wpedantic -Wshadow -Wstrict-aliasing
		-Wunreachable-code -Wdouble-promotion -Weffc++ -Wconversion
		-Wsign-conversion
		"$<$<CONFIG:DEBUG>:-Og;-ggdb;-Werror>"
		"$<$<CONFIG:RELEASE>:-Ofast>"
	)
endif()
