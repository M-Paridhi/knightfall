cmake_minimum_required(VERSION 3.16)
project(Knightfall CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/O2 /W3)
else()
    add_compile_options(-O3 -mpopcnt -Wall)
endif()

set(CORE_SOURCES
    src/core/bitboard.cpp
    src/core/board.cpp
    src/core/attacks.cpp
    src/core/movegen.cpp
)

set(SEARCH_SOURCES
    src/search/evaluate.cpp
    src/search/search.cpp
    src/search/tt.cpp
)

# Main UCI engine
add_executable(knightfall
    src/main.cpp
    src/protocol/uci.cpp
    ${CORE_SOURCES}
    ${SEARCH_SOURCES}
)
target_include_directories(knightfall PRIVATE src)

# Perft correctness tests
add_executable(perft
    tests/perft.cpp
    ${CORE_SOURCES}
)
target_include_directories(perft PRIVATE src)
