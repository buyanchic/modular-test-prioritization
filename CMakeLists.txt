cmake_minimum_required(VERSION 3.16)
project(tcp LANGUAGES CXX VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(TCP_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

add_executable(tcp src/main.cpp)
target_include_directories(tcp PRIVATE src)

enable_testing()

add_executable(tcp_tests tests/tcp_tests.cpp)
target_include_directories(tcp_tests PRIVATE src)
target_compile_definitions(tcp_tests PRIVATE TCP_DATA_DIR="${TCP_DATA_DIR}")

add_test(NAME unit COMMAND tcp_tests)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME reference
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_reference.py
                   --exe $<TARGET_FILE:tcp>
                   --input ${TCP_DATA_DIR}/rothermel2001.json
                   --expected ${TCP_DATA_DIR}/rothermel2001.expected.json)
endif()
