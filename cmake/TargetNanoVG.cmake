set(NANOVG_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extern/nanovg/src")

add_library(nanovg STATIC "${NANOVG_SOURCE_DIR}/nanovg.c")

target_compile_options(nanovg PRIVATE -O3 -DNDEBUG)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
target_link_libraries(nanovg OpenGL::OpenGL)
target_compile_definitions(nanovg PUBLIC NANOVG_GL3_IMPLEMENTATION)

target_include_directories(nanovg SYSTEM PUBLIC "${NANOVG_SOURCE_DIR}")

set_target_properties(nanovg PROPERTIES
	POSITION_INDEPENDENT_CODE ON
	C_VISIBILITY_PRESET hidden
)
