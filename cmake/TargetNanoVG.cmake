set(NANOVG_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extern/nanovg/src")

add_library(nanovg STATIC "${NANOVG_SOURCE_DIR}/nanovg.c")

target_compile_definitions(nanovg
	PUBLIC NANOVG_GL3_IMPLEMENTATION
	PRIVATE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS
)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
target_link_libraries(nanovg OpenGL::GL)

target_include_directories(nanovg SYSTEM PUBLIC "${NANOVG_SOURCE_DIR}")

set_target_properties(nanovg PROPERTIES
	POSITION_INDEPENDENT_CODE ON
	C_VISIBILITY_PRESET hidden
)
