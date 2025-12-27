
cmake_minimum_required(VERSION 3.10)
project(BettiOS_Kernel VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

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
if(NOT MSVC)
    target_link_libraries(bettios_kernel atomic)
endif()

# Hanoi demo executable
add_executable(hanoi_demo demos/hanoi_demo.cpp)
if(NOT MSVC)
    target_link_libraries(hanoi_demo atomic)
endif()

# Parallel Hanoi test executable
add_executable(parallel_hanoi_test demos/parallel_hanoi_test.cpp)
if(NOT MSVC)
    target_link_libraries(parallel_hanoi_test atomic)
endif()

# RSE-native Hanoi executable
add_executable(rse_native_hanoi demos/rse_native_hanoi.cpp)
if(NOT MSVC)
    target_link_libraries(rse_native_hanoi atomic)
endif()

# Betti-integrated Hanoi executable
add_executable(betti_hanoi_integrated demos/betti_hanoi_integrated.cpp)
if(NOT MSVC)
    target_link_libraries(betti_hanoi_integrated atomic)
endif()

# Betti-RDL demo executable
add_executable(betti_rdl_demo demos/betti_rdl_demo.cpp)
if(NOT MSVC)
    target_link_libraries(betti_rdl_demo atomic)
endif()

# Betti-RDL stress test executable
add_executable(betti_rdl_stress_test demos/betti_rdl_stress_test.cpp)
if(NOT MSVC)
    target_link_libraries(betti_rdl_stress_test atomic)
endif()

# Adaptive delay validation executable
add_executable(adaptive_delay_validation demos/adaptive_delay_validation.cpp)
if(NOT MSVC)
    target_link_libraries(adaptive_delay_validation atomic)
endif()

# Distributed counter demo
add_executable(distributed_counter demos/distributed_counter.cpp)
if(NOT MSVC)
    target_link_libraries(distributed_counter atomic)
endif()

# OS userspace init demo
add_executable(os_userspace_init demos/os_userspace_init.cpp)
if(NOT MSVC)
    target_link_libraries(os_userspace_init atomic)
endif()

# Parallel scaling test
add_executable(parallel_scaling_test_v2 demos/parallel_scaling_test.cpp)
if(NOT MSVC)
    target_link_libraries(parallel_scaling_test_v2 atomic)
endif()

# C API shared library for FFI bindings
add_library(betti_rdl_c SHARED betti_rdl_c_api.cpp)
target_compile_definitions(betti_rdl_c PRIVATE BETTI_RDL_EXPORTS)
if(NOT MSVC)
    target_link_libraries(betti_rdl_c atomic)
endif()

# C API test (C wrapper for C++ library - link with C++)
add_executable(c_api_test tests/c_api_test.c)
target_link_libraries(c_api_test betti_rdl_c)
if(NOT MSVC)
    target_link_libraries(c_api_test atomic)
endif()

# Allocator test (multi-threaded bounded arena allocator tests)
add_executable(allocator_test tests/allocator_test.cpp)
if(NOT MSVC)
    target_link_libraries(allocator_test pthread atomic)
endif()

# Fixed structures regression tests
add_executable(fixed_structures_test tests/fixed_structures_test.cpp)
if(NOT MSVC)
    target_link_libraries(fixed_structures_test pthread atomic)
endif()

# Thread-safe scheduler test
add_executable(threadsafe_scheduler_test tests/threadsafe_scheduler_test.cpp)
if(NOT MSVC)
    target_link_libraries(threadsafe_scheduler_test pthread atomic)
endif()

# ELF loader parser test
add_executable(elf_loader_test tests/elf_loader_test.cpp)
if(NOT MSVC)
    target_link_libraries(elf_loader_test atomic)
endif()

# ELF process loader test
add_executable(elf_process_test tests/elf_process_test.cpp)
if(NOT MSVC)
    target_link_libraries(elf_process_test atomic)
endif()

# Exec + VFS integration test
add_executable(exec_vfs_test tests/exec_vfs_test.cpp)
if(NOT MSVC)
    target_link_libraries(exec_vfs_test atomic)
endif()

# sys_wait reaping test
add_executable(sys_wait_test tests/sys_wait_test.cpp)
if(NOT MSVC)
    target_link_libraries(sys_wait_test atomic)
endif()

# Memory Telemetry Test
add_executable(memory_telemetry_test tests/memory_telemetry_test.cpp)
if(NOT MSVC)
    target_link_libraries(memory_telemetry_test pthread atomic)
endif()

# Killer Demo Memory Test
add_executable(killer_demo_memory_test tests/killer_demo_memory_test.cpp)
if(NOT MSVC)
    target_link_libraries(killer_demo_memory_test pthread atomic)
endif()

# Stress Test
add_executable(stress_test benchmarks/stress_test.cpp)
target_link_libraries(stress_test betti_rdl_c) 
if(NOT MSVC)
    target_link_libraries(stress_test atomic)
endif()

# Mega Scale Demos
add_executable(mega_demo demos/scale_demos/mega_demo.cpp)
target_link_libraries(mega_demo betti_rdl_c)
if(NOT MSVC)
    target_link_libraries(mega_demo atomic)
endif()

# JSON reference demo for Grey comparison harness
add_executable(grey_sir_reference demos/scale_demos/grey_sir_reference.cpp)
if(NOT MSVC)
    target_link_libraries(grey_sir_reference atomic)
endif()

# Tests
add_test(NAME allocator_test COMMAND allocator_test)
add_test(NAME fixed_structures_test COMMAND fixed_structures_test)
add_test(NAME c_api_test COMMAND c_api_test)
add_test(NAME threadsafe_scheduler_test COMMAND threadsafe_scheduler_test)
add_test(NAME memory_telemetry_test COMMAND memory_telemetry_test)
add_test(NAME killer_demo_memory_test COMMAND killer_demo_memory_test)

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
    target_compile_options(os_userspace_init PRIVATE /W4)
    target_compile_options(parallel_scaling_test_v2 PRIVATE /W4)
    target_compile_options(allocator_test PRIVATE /W4)
    target_compile_options(fixed_structures_test PRIVATE /W4)
    target_compile_options(threadsafe_scheduler_test PRIVATE /W4)
    target_compile_options(memory_telemetry_test PRIVATE /W4)
    target_compile_options(killer_demo_memory_test PRIVATE /W4)
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
    target_compile_definitions(allocator_test PRIVATE $<$<CONFIG:Release>:NDEBUG>)
    target_compile_definitions(fixed_structures_test PRIVATE $<$<CONFIG:Release>:NDEBUG>)

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
    target_compile_options(os_userspace_init PRIVATE -O3 -Wall -Wextra)
    target_compile_options(parallel_scaling_test_v2 PRIVATE -O3 -Wall -Wextra)
    target_compile_options(allocator_test PRIVATE -O3 -Wall -Wextra)
    target_compile_options(fixed_structures_test PRIVATE -O3 -Wall -Wextra)
    target_compile_options(threadsafe_scheduler_test PRIVATE -O3 -Wall -Wextra)
    target_compile_options(memory_telemetry_test PRIVATE -O3 -Wall -Wextra)
    target_compile_options(killer_demo_memory_test PRIVATE -O3 -Wall -Wextra)
endif()
