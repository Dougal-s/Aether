cmake_minimum_required(VERSION 3.10)

project(Aether VERSION 0.1.0)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_EXTENSIONS false)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	include_directories(SYSTEM /usr/local/include)
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

target_compile_options(aether_dsp PRIVATE
	-Wall -Wextra -Wpedantic -Wshadow -Weffc++ -Wstrict-aliasing
	-Wunreachable-code -Wdouble-promotion -Werror -Wconversion -Wsign-conversion
	"$<$<CONFIG:DEBUG>:-O0;-ggdb>"
	"$<$<CONFIG:RELEASE>:-O3>"
)
target_compile_definitions(aether_dsp PRIVATE "$<$<CONFIG:RELEASE>:NDEBUG>")

set_target_properties(aether_dsp PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(aether_dsp PROPERTIES PREFIX "")

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

target_compile_options(aether_ui PRIVATE
	-Wall -Wextra -Wpedantic -Wshadow -Wstrict-aliasing
	-Wunreachable-code -Wdouble-promotion -Werror
	"$<$<CONFIG:DEBUG>:-O0;-ggdb>"
	"$<$<CONFIG:RELEASE>:-O3>"
)
target_compile_definitions(aether_ui PRIVATE "$<$<CONFIG:RELEASE>:NDEBUG>")

set_target_properties(aether_ui PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(aether_ui PROPERTIES PREFIX "")

include(TargetGlad)
target_link_libraries(aether_ui PRIVATE glad)
include(TargetPugl)
target_link_libraries(aether_ui PRIVATE pugl)
include(TargetNanoVG)
target_link_libraries(aether_ui PRIVATE nanovg)

add_custom_command(TARGET aether_dsp POST_BUILD COMMAND cp aether_dsp.so ../aether.lv2/)
add_custom_command(TARGET aether_ui POST_BUILD COMMAND cp aether_ui.so ../aether.lv2/)

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
