cmake_minimum_required(VERSION 3.10)

project(Aether VERSION 1.2.1)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_EXTENSIONS false)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	include_directories(SYSTEM /usr/local/include)
	set(UI_TYPE "CocoaUI")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(UI_TYPE "X11UI")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
	set(UI_TYPE "WindowsUI")
else()
	message(FATAL_ERROR "Unrecognized Platform!")
endif()


if (MSVC)
	add_compile_options("$<$<CONFIG:DEBUG>:/W4>")
else()
	add_compile_options(
		-Wall -Wextra -pedantic -Wshadow -Wstrict-aliasing -Wunreachable-code -Wdouble-promotion
		"$<$<CONFIG:DEBUG>:-Og;-ggdb;-Werror>"
	)
endif()


add_library(aether_dsp MODULE
	src/DSP/aether_dsp_lv2.cpp
	src/DSP/aether_dsp.cpp
	src/DSP/aether_dsp.hpp
	src/DSP/delay.hpp
	src/DSP/filters.hpp
	src/DSP/diffuser.hpp
	src/DSP/utils/lfo.hpp
	src/DSP/utils/random.hpp
	src/DSP/utils/ringbuffer.hpp
)

add_library(aether_ui MODULE
	src/UI/aether_ui.cpp
	src/UI/aether_ui_lv2.cpp
	src/UI/aether_ui.hpp
	src/UI/gl_helper.cpp
	src/UI/gl_helper.hpp
	src/UI/ui_tree.cpp
	src/UI/ui_tree.hpp
	src/common/parameters.hpp
)

include(TargetGlad)
target_link_libraries(aether_ui PRIVATE glad)
include(TargetPugl)
target_link_libraries(aether_ui PRIVATE pugl)
include(TargetNanoVG)
target_link_libraries(aether_ui PRIVATE nanovg)

set_target_properties(aether_dsp aether_ui
	PROPERTIES
	CXX_VISIBILITY_PRESET hidden
	LIBRARY_OUTPUT_DIRECTORY aether.lv2
	PREFIX "")

if (MSVC)
else()
	target_compile_options(aether_dsp PRIVATE -Weffc++ -Wconversion -Wsign-conversion)
endif()


# create Aether.lv2
add_custom_target(copy_fonts ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/fonts aether.lv2/fonts)

configure_file(resources/manifest.ttl.in aether.lv2/manifest.ttl)
configure_file(resources/aether.ttl.in aether.lv2/aether.ttl)

# Tests
option(BUILD_TESTS "Build tests" OFF)

if (BUILD_TESTS)
	set(BUILD_GMOCK OFF CACHE BOOL "Builds the googlemock subproject")
	set(INSTALL_GTEST OFF CACHE BOOL "Enable installation of googletest. (Projects embedding googletest may want to turn this OFF.)")
	add_subdirectory("${CMAKE_SOURCE_DIR}/extern/googletest")

	enable_testing()
	add_subdirectory(tests/tests)
endif()

option(BUILD_BENCHMARKS "Build benchmarks" OFF)

if (BUILD_BENCHMARKS)
	set(BENCHMARK_ENABLE_LTO OFF CACHE BOOL "Enable link time optimisation of the benchmark library.")
	set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Enable testing of the benchmark library.")
	add_subdirectory("${CMAKE_SOURCE_DIR}/extern/benchmark")

	enable_testing()
	add_subdirectory(tests/benchmarks)
endif()
