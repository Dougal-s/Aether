cmake_minimum_required(VERSION 3.10)

project(Aether VERSION 1.2.1)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

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


add_subdirectory(src/DSP)

option(BUILD_GUI "Build user interface" ON)

if (BUILD_GUI)
	add_subdirectory(src/UI)
endif()


if (DEFINED LV2_INCLUDE_PATH)
	target_include_directories(aether_dsp SYSTEM PRIVATE ${LV2_INCLUDE_PATH})
	target_include_directories(aether_ui SYSTEM PRIVATE ${LV2_INCLUDE_PATH})
else()
	target_include_directories(aether_dsp SYSTEM PRIVATE extern/lv2)
	target_include_directories(aether_ui SYSTEM PRIVATE extern/lv2)
endif()


# create Aether.lv2
add_custom_target(copy_fonts ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/fonts aether.lv2/fonts)

add_custom_target(copy_dsp_binaries ALL
	${CMAKE_COMMAND} -E copy "$<TARGET_FILE:aether_dsp>" aether.lv2/
	DEPENDS aether_dsp
)

if (BUILD_GUI)
	add_custom_target(copy_ui_binaries ALL
		${CMAKE_COMMAND} -E copy "$<TARGET_FILE:aether_ui>" aether.lv2/
		DEPENDS aether_ui
	)
endif()

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
