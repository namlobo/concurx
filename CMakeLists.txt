cmake_minimum_required(VERSION 3.10)
project(ConcurX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(concurx_core
    src/LogParser.cpp
    src/Statistics.cpp
    src/LogProcessor.cpp
)
target_include_directories(concurx_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(concurx_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(concurx_core PRIVATE /W4)
else()
    target_compile_options(concurx_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(concurx src/main.cpp)
target_link_libraries(concurx PRIVATE concurx_core)

add_executable(concurx_tests tests/test_main.cpp)
target_link_libraries(concurx_tests PRIVATE concurx_core)

enable_testing()
add_test(NAME concurx_tests COMMAND concurx_tests)
