set(GLAD_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extern/glad")

add_library(glad STATIC "${GLAD_SOURCE_DIR}/glad/glad.c")

target_include_directories(glad SYSTEM PUBLIC "${GLAD_SOURCE_DIR}")

set_target_properties(glad PROPERTIES
	POSITION_INDEPENDENT_CODE ON
	C_VISIBILITY_PRESET hidden
)
