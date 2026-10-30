cmake_minimum_required(VERSION 3.16)
project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Both engines; consumers get the public headers plus BookNaive.hpp at the root.
add_library(orderbook STATIC
  src/BookNaive.cpp
  src/BookFast.cpp
)
target_include_directories(orderbook PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# The test source is compiled once per engine.
add_executable(test_book_naive tests/test_book.cpp)
target_link_libraries(test_book_naive PRIVATE orderbook)

add_executable(test_book_fast tests/test_book.cpp)
target_compile_definitions(test_book_fast PRIVATE BOOK_TEST_FAST)
target_link_libraries(test_book_fast PRIVATE orderbook)

# Same benchmark harness, engine selected at build time.
add_executable(bench_book bench/bench.cpp)
target_link_libraries(bench_book PRIVATE orderbook)

add_executable(bench_book_fast bench/bench.cpp)
target_compile_definitions(bench_book_fast PRIVATE BENCH_BOOK_FAST)
target_link_libraries(bench_book_fast PRIVATE orderbook)

# Python bindings are optional: skipped when pybind11 is not installed.
option(ORDERBOOK_BUILD_PYTHON "Build the pybind11 bindings" ON)
if(ORDERBOOK_BUILD_PYTHON)
  find_package(pybind11 CONFIG QUIET)
  if(pybind11_FOUND)
    pybind11_add_module(bookfast bindings/module.cpp)
    target_link_libraries(bookfast PRIVATE orderbook)
  else()
    message(STATUS "pybind11 not found - skipping bindings/module.cpp")
  endif()
endif()

enable_testing()
add_test(NAME test_book_naive COMMAND test_book_naive)
add_test(NAME test_book_fast COMMAND test_book_fast)
