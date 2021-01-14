macro(create_test TEST_NAME)
	add_executable(test_${TEST_NAME} ${ARGN})

	target_compile_options(test_${TEST_NAME} PRIVATE -O3)

	target_include_directories(test_${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_link_libraries(test_${TEST_NAME} gtest gtest_main)
	add_test(${TEST_NAME}_test test_${TEST_NAME})
endmacro()


macro(create_benchmark BENCHMARK_NAME)
	add_executable(bm_${BENCHMARK_NAME} ${ARGN})

	target_compile_options(bm_${BENCHMARK_NAME} PRIVATE -O3)
	target_compile_definitions(bm_${BENCHMARK_NAME} PRIVATE NDEBUG)

	target_include_directories(bm_${BENCHMARK_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_link_libraries(bm_${BENCHMARK_NAME} benchmark::benchmark benchmark::benchmark_main)
	add_test(${BENCHMARK_NAME}_benchmark bm_${BENCHMARK_NAME})
endmacro()

create_test(rng
	test_rng.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/utils/random.hpp
)

create_test(lfo
	test_lfo.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/utils/lfo.hpp
)

create_test(diffuser
	test_diffuser.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/diffuser.hpp
)


create_benchmark(filters
	bm_filters.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/filters.hpp
)

create_benchmark(aether
	bm_aether.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/aether_dsp.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/aether_dsp.hpp
	${PROJECT_SOURCE_DIR}/src/DSP/delay.hpp
	${PROJECT_SOURCE_DIR}/src/DSP/diffuser.hpp
	${PROJECT_SOURCE_DIR}/src/DSP/filters.hpp
)

create_benchmark(diffuser
	bm_diffuser.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/diffuser.hpp
)

create_benchmark(delay
	bm_delay.cpp
	${PROJECT_SOURCE_DIR}/src/DSP/delay.hpp
)
