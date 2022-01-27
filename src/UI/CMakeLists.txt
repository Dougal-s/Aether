add_library(aether_ui MODULE
	aether_ui_lv2.cpp
	aether_ui.cpp
	aether_ui.hpp
	gl_helper.cpp
	gl_helper.hpp
	style.hpp
	ui_tree.cpp
	ui_tree.hpp
)

target_compile_features(aether_ui PUBLIC cxx_std_20)

set_target_properties(aether_ui
	PROPERTIES
	CXX_VISIBILITY_PRESET hidden
	INTERPROCEDURAL_OPTIMIZATION TRUE
	PREFIX ""
)

# Dependencies

include(TargetGlad)
target_link_libraries(aether_ui PRIVATE glad)
include(TargetPugl)
target_link_libraries(aether_ui PRIVATE pugl)
include(TargetNanoVG)
target_link_libraries(aether_ui PRIVATE nanovg)


# Compile Options

target_compile_definitions(aether_ui PRIVATE "$<$<CONFIG:RELEASE>:NDEBUG>")

# Platform

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_definitions(aether_ui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (MSVC)
	target_compile_options(aether_ui PRIVATE
		"$<$<CONFIG:DEBUG>:/W4>"
		"$<$<CONFIG:RELEASE>:/O2>"
	)
else()
	target_compile_options(aether_ui PRIVATE
		-Wall -Wextra -Wpedantic -Wshadow -Wstrict-aliasing
		-Wunreachable-code -Wdouble-promotion
		"$<$<CONFIG:DEBUG>:-Og;-ggdb;-Werror>"
		"$<$<CONFIG:RELEASE>:-O3>"
	)
endif()
