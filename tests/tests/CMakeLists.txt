macro(create_test TEST_NAME)
	add_executable(test_${TEST_NAME} ${ARGN})

	target_compile_features(test_${TEST_NAME} PUBLIC cxx_std_17)

	target_include_directories(test_${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_link_libraries(test_${TEST_NAME} gtest gtest_main)
	add_test(${TEST_NAME}_test test_${TEST_NAME})
endmacro()

create_test(rng
	test_rng.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/utils/random.hpp
)

create_test(lfo
	test_lfo.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/utils/lfo.hpp
)

create_test(common
	test_common.cpp
	${PROJECT_SOURCE_DIR}/src/common/bit_ops.hpp
	${PROJECT_SOURCE_DIR}/src/common/constants.hpp
)

create_test(diffuser
	test_diffuser.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/diffuser.hpp
)

create_test(output
	test_output.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/aether_dsp.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/aether_dsp.hpp
	${PROJECT_SOURCE_DIR}/src/DSP/delay.hpp
	${PROJECT_SOURCE_DIR}/src/DSP/diffuser.hpp
	${PROJECT_SOURCE_DIR}/src/DSP/filters.hpp
)
