cmake_minimum_required(VERSION 3.16)
project(Knightfall CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use Release build for performance
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/O2)
else()
    add_compile_options(-O3 -mpopcnt)
endif()

add_executable(knightfall
    src/main.cpp
    src/core/bitboard.cpp
    src/core/board.cpp
)

target_include_directories(knightfall PRIVATE src)