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

# ── UCI engine ────────────────────────────
add_executable(knightfall
    src/main.cpp
    src/protocol/uci.cpp
    ${CORE_SOURCES}
    ${SEARCH_SOURCES}
)
target_include_directories(knightfall PRIVATE src)

# ── Perft tests ───────────────────────────
add_executable(perft
    tests/perft.cpp
    ${CORE_SOURCES}
)
target_include_directories(perft PRIVATE src)

# ── Terminal game ─────────────────────────
add_executable(play
    src/play.cpp
    ${CORE_SOURCES}
    ${SEARCH_SOURCES}
)
target_include_directories(play PRIVATE src)

# ── Web visualizer ────────────────────────
add_executable(visualizer
    src/vis_main.cpp
    src/protocol/server.cpp
    ${CORE_SOURCES}
    ${SEARCH_SOURCES}
)
target_include_directories(visualizer PRIVATE src)

# Threading: Winsock + pthreads on Windows, pthreads on Linux
if(WIN32)
    target_link_libraries(knightfall   ws2_32)
    target_link_libraries(visualizer   ws2_32 -static -lpthread)
else()
    target_link_libraries(visualizer   pthread)
endif()
