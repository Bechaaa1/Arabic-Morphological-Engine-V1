cmake_minimum_required(VERSION 3.14)
project(ArabicMorphoEngine
    VERSION 1.0.0
    DESCRIPTION "Arabic Morphological Search Engine – AVL Tree + Hash Table"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -fsanitize=address,undefined -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# ─── Source files ──────────────────────────────────────────────────────────────
set(SOURCES
    src/ArabicUtils.cpp
    src/AVLTree.cpp
    src/HashTable.cpp
    src/MorphologyEngine.cpp
    src/HttpServer.cpp
    src/main.cpp
)

# ─── Main executable ───────────────────────────────────────────────────────────
add_executable(arabic-morpho ${SOURCES})
target_include_directories(arabic-morpho PRIVATE include)

# POSIX threading (for future multi-threading if extended)
find_package(Threads REQUIRED)
target_link_libraries(arabic-morpho PRIVATE Threads::Threads)

# ─── Install ───────────────────────────────────────────────────────────────────
install(TARGETS arabic-morpho RUNTIME DESTINATION bin)
install(DIRECTORY data/ DESTINATION share/arabic-morpho/data)

# ─── Print summary ─────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "========================================")
message(STATUS " Arabic Morphological Engine")
message(STATUS " Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS " C++ std    : C++${CMAKE_CXX_STANDARD}")
message(STATUS " Output     : arabic-morpho")
message(STATUS "========================================")
message(STATUS "")
message(STATUS " Run modes:")
message(STATUS "   ./arabic-morpho              → HTTP server :8080")
message(STATUS "   ./arabic-morpho --cli        → Interactive CLI")
message(STATUS "   ./arabic-morpho --demo       → Demonstration")
message(STATUS "   ./arabic-morpho --port 9000  → Custom port")
message(STATUS "")
