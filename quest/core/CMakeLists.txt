# Host build of the Unity-free Quest core + its tests (no Android or game needed).
#   cmake -S quest/core -B build/core-tests -DRAPIDJSON_INCLUDE_DIR=<path>/rapidjson/include
#   cmake --build build/core-tests && ./build/core-tests/saucy_core_tests
cmake_minimum_required(VERSION 3.22)
project(saucy_core CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT RAPIDJSON_INCLUDE_DIR)
  message(FATAL_ERROR "Set RAPIDJSON_INCLUDE_DIR to a rapidjson/include folder (the Quest build uses beatsaber-hook's copy).")
endif()

add_library(saucy_core STATIC
  src/module.cpp src/room_gate.cpp src/registry.cpp src/search.cpp src/theme.cpp src/settings.cpp src/menu.cpp)
target_include_directories(saucy_core PUBLIC include)
target_include_directories(saucy_core PRIVATE ${RAPIDJSON_INCLUDE_DIR})
target_compile_options(saucy_core PRIVATE -Wall -Wextra -Werror)

add_executable(saucy_core_tests tests/test_main.cpp)
target_link_libraries(saucy_core_tests PRIVATE saucy_core)
target_compile_options(saucy_core_tests PRIVATE -Wall -Wextra)
