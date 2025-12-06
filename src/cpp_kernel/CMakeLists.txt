
cmake_minimum_required(VERSION 3.10)
project(BettiOS_Kernel VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    Microkernel.cpp
)

# Headers
include_directories(.)

# Executable
add_executable(bettios_kernel ${SOURCES})

# Compiler Flags (Optimization + Warnings)
if(MSVC)
    target_compile_options(bettios_kernel PRIVATE /O2 /W4)
else()
    target_compile_options(bettios_kernel PRIVATE -O3 -Wall -Wextra)
endif()
