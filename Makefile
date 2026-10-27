CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -Wpedantic
BUILD_DIR := build

ifeq ($(origin CXX), default)
CXX := clang++
endif

TEST_SOURCES := tests/test_book.cpp book.cpp
TEST_HEADERS := book.hpp order.hpp
TEST_BINARY := $(BUILD_DIR)/test_book

BENCH_SOURCES := bench/bench.cpp book.cpp
BENCH_BINARY := $(BUILD_DIR)/bench_book

.PHONY: all test bench clean

all: test

$(TEST_BINARY): $(TEST_SOURCES) $(TEST_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. -o $@ $(TEST_SOURCES)

test: $(TEST_BINARY)
	./$(TEST_BINARY)

$(BENCH_BINARY): $(BENCH_SOURCES) $(TEST_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. -o $@ $(BENCH_SOURCES)

bench: $(BENCH_BINARY)
	./$(BENCH_BINARY)

clean:
	rm -rf $(BUILD_DIR)
