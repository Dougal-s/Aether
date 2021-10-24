set(PUGL_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extern/pugl")

add_library(pugl STATIC "${PUGL_SOURCE_DIR}/src/implementation.c")

target_compile_definitions(pugl PUBLIC PUGL_INTERNAL PUGL_STATIC)

target_include_directories(pugl SYSTEM PUBLIC
	"${PUGL_SOURCE_DIR}/include"
	"${PUGL_SOURCE_DIR}/bindings/cxx/include"
)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
target_link_libraries(pugl OpenGL::GL)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_sources(pugl PRIVATE "${PUGL_SOURCE_DIR}/src/mac.m" "${PUGL_SOURCE_DIR}/src/mac_gl.m")
	target_link_libraries(pugl "-framework Cocoa -framework CoreVideo")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_sources(pugl PRIVATE "${PUGL_SOURCE_DIR}/src/x11.c" "${PUGL_SOURCE_DIR}/src/x11_gl.c")
	find_library(X11 X11)
	target_link_libraries(pugl ${X11})
	target_link_libraries(pugl OpenGL::GLX)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_sources(pugl PRIVATE "${PUGL_SOURCE_DIR}/src/win.c" "${PUGL_SOURCE_DIR}/src/win_gl.c")
	target_compile_definitions(pugl PRIVATE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
	message(FATAL_ERROR "Unrecognized Platform!")
endif()

set_target_properties(pugl PROPERTIES
	POSITION_INDEPENDENT_CODE ON
	C_VISIBILITY_PRESET hidden
	CXX_VISIBILITY_PRESET hidden
)
