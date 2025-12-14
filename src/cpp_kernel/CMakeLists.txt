
cmake_minimum_required(VERSION 3.10)
project(BettiOS_Kernel VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use Release build by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Source files
set(SOURCES
    Microkernel.cpp
)

# Headers
include_directories(.)

# Main kernel executable
add_executable(bettios_kernel ${SOURCES})

# Hanoi demo executable
add_executable(hanoi_demo demos/hanoi_demo.cpp)

# Parallel Hanoi test executable
add_executable(parallel_hanoi_test demos/parallel_hanoi_test.cpp)

# RSE-native Hanoi executable
add_executable(rse_native_hanoi demos/rse_native_hanoi.cpp)

# Betti-integrated Hanoi executable
add_executable(betti_hanoi_integrated demos/betti_hanoi_integrated.cpp)

# Betti-RDL demo executable
add_executable(betti_rdl_demo demos/betti_rdl_demo.cpp)

# Betti-RDL stress test executable
add_executable(betti_rdl_stress_test demos/betti_rdl_stress_test.cpp)

# Adaptive delay validation executable
add_executable(adaptive_delay_validation demos/adaptive_delay_validation.cpp)

# Distributed counter demo
add_executable(distributed_counter demos/distributed_counter.cpp)

# Parallel scaling test
add_executable(parallel_scaling_test_v2 demos/parallel_scaling_test.cpp)

# C API shared library for FFI bindings
add_library(betti_rdl_c SHARED betti_rdl_c_api.cpp)
target_compile_definitions(betti_rdl_c PRIVATE BETTI_RDL_EXPORTS)

# C API test
add_executable(c_api_test tests/c_api_test.c)
target_link_libraries(c_api_test betti_rdl_c)
set_target_properties(c_api_test PROPERTIES LINKER_LANGUAGE CXX)

# Stress Test
add_executable(stress_test benchmarks/stress_test.cpp)
target_link_libraries(stress_test betti_rdl_c) 

# Mega Scale Demos
add_executable(mega_demo demos/scale_demos/mega_demo.cpp)
target_link_libraries(mega_demo betti_rdl_c)

# Compiler Flags (Optimization + Warnings)
if(MSVC)
    target_compile_options(bettios_kernel PRIVATE /W4)
    target_compile_options(hanoi_demo PRIVATE /W4)
    target_compile_options(parallel_hanoi_test PRIVATE /W4)
    target_compile_options(rse_native_hanoi PRIVATE /W4)
    target_compile_options(betti_hanoi_integrated PRIVATE /W4)
    target_compile_options(betti_rdl_demo PRIVATE /W4)
    target_compile_options(betti_rdl_stress_test PRIVATE /W4)
    target_compile_options(adaptive_delay_validation PRIVATE /W4)
    target_compile_options(distributed_counter PRIVATE /W4)
    target_compile_options(parallel_scaling_test_v2 PRIVATE /W4)
    # Disable runtime checks in Release mode to avoid conflict with /O2
    target_compile_definitions(bettios_kernel PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(hanoi_demo PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(parallel_hanoi_test PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(rse_native_hanoi PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(betti_hanoi_integrated PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(betti_rdl_demo PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(betti_rdl_stress_test PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(adaptive_delay_validation PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(distributed_counter PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(parallel_scaling_test_v2 PRIVATE $<$<CONFIG:Release>:NDEBUG>)
else()
    target_compile_options(bettios_kernel PRIVATE -O3 -Wall -Wextra)
    target_compile_options(hanoi_demo PRIVATE -O3 -Wall -Wextra)
    target_compile_options(parallel_hanoi_test PRIVATE -O3 -Wall -Wextra)
    target_compile_options(rse_native_hanoi PRIVATE -O3 -Wall -Wextra)
    target_compile_options(betti_hanoi_integrated PRIVATE -O3 -Wall -Wextra)
    target_compile_options(betti_rdl_demo PRIVATE -O3 -Wall -Wextra)
    target_compile_options(betti_rdl_stress_test PRIVATE -O3 -Wall -Wextra)
    target_compile_options(adaptive_delay_validation PRIVATE -O3 -Wall -Wextra)
    target_compile_options(distributed_counter PRIVATE -O3 -Wall -Wextra)
    target_compile_options(parallel_scaling_test_v2 PRIVATE -O3 -Wall -Wextra)
endif()
