cmake_minimum_required(VERSION 3.16)
project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
  if(NOT pybind11_FOUND)
    # A pip-installed pybind11 is not on CMake's search path; ask it where it is.
    find_package(Python3 COMPONENTS Interpreter Development.Module QUIET)
    if(Python3_Interpreter_FOUND)
      execute_process(
        COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
        OUTPUT_VARIABLE _pybind11_cmakedir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _pybind11_lookup
      )
      if(_pybind11_lookup EQUAL 0)
        list(APPEND CMAKE_PREFIX_PATH "${_pybind11_cmakedir}")
        find_package(pybind11 CONFIG QUIET)
      endif()
    endif()
  endif()

  if(pybind11_FOUND)
    # pybind11's modern tools drive FindPython3, so the dev headers are needed.
    find_package(Python3 COMPONENTS Interpreter Development.Module QUIET)
    if(Python3_FOUND)
      pybind11_add_module(book bindings/module.cpp)
      target_link_libraries(book PRIVATE orderbook)
    else()
      message(STATUS "Python development headers not found - skipping bindings/module.cpp")
    endif()
  else()
    message(STATUS "pybind11 not found - skipping bindings/module.cpp")
  endif()
endif()

enable_testing()
add_test(NAME test_book_naive COMMAND test_book_naive)
add_test(NAME test_book_fast COMMAND test_book_fast)
