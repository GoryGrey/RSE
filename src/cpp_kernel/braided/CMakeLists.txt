cmake_minimum_required(VERSION 3.14)

# Braided-RSE: Three-Torus Braided System
project(BraidedRSE CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Braided demo executable
add_executable(braided_demo
    ../demos/braided_demo.cpp
)

# Comprehensive test executable
add_executable(test_braided
    ../demos/test_braided_comprehensive.cpp
)

# Link with pthread for std::thread support
find_package(Threads REQUIRED)
target_link_libraries(braided_demo Threads::Threads atomic)
target_link_libraries(test_braided Threads::Threads atomic)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(braided_demo PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O3
    )
endif()

message(STATUS "Braided-RSE build configured")
message(STATUS "  - C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  - Build Type: ${CMAKE_BUILD_TYPE}")
