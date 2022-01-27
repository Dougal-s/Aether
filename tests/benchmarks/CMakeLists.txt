macro(create_benchmark BENCHMARK_NAME)
	add_executable(bm_${BENCHMARK_NAME} ${ARGN})

	target_compile_features(bm_${BENCHMARK_NAME} PUBLIC cxx_std_17)

	if (MSVC)
		target_compile_options(bm_${BENCHMARK_NAME} PRIVATE /O2)
	else()
		target_compile_options(bm_${BENCHMARK_NAME} PRIVATE -O3)
	endif()

	target_compile_definitions(bm_${BENCHMARK_NAME} PRIVATE NDEBUG)

	target_include_directories(bm_${BENCHMARK_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_link_libraries(bm_${BENCHMARK_NAME} benchmark::benchmark benchmark::benchmark_main)
	add_test(${BENCHMARK_NAME}_benchmark bm_${BENCHMARK_NAME})
endmacro()


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
