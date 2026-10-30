CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -Wpedantic
BUILD_DIR := build

ifeq ($(origin CXX), default)
CXX := clang++
endif

ENGINE_SOURCES := BookNaive.cpp BookFast.cpp
TEST_SOURCES := tests/test_book.cpp $(ENGINE_SOURCES)
TEST_HEADERS := BookNaive.hpp BookFast.hpp order.hpp

TEST_BINARY := $(BUILD_DIR)/test_book
FAST_TEST_BINARY := $(BUILD_DIR)/test_book_fast

BENCH_SOURCES := bench/bench.cpp $(ENGINE_SOURCES)
BENCH_BINARY := $(BUILD_DIR)/bench_book
FAST_BENCH_BINARY := $(BUILD_DIR)/bench_book_fast

.PHONY: all test test-naive test-fast bench bench-naive bench-fast clean

all: test

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(TEST_BINARY): $(TEST_SOURCES) $(TEST_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. -o $@ $(TEST_SOURCES)

$(FAST_TEST_BINARY): $(TEST_SOURCES) $(TEST_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -DBOOK_TEST_FAST -I. -o $@ $(TEST_SOURCES)

test: test-naive test-fast

test-naive: $(TEST_BINARY)
	./$(TEST_BINARY)

test-fast: $(FAST_TEST_BINARY)
	./$(FAST_TEST_BINARY)

$(BENCH_BINARY): $(BENCH_SOURCES) $(TEST_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. -o $@ $(BENCH_SOURCES)

$(FAST_BENCH_BINARY): $(BENCH_SOURCES) $(TEST_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -DBENCH_BOOK_FAST -I. -o $@ $(BENCH_SOURCES)

bench: bench-naive bench-fast

bench-naive: $(BENCH_BINARY)
	./$(BENCH_BINARY)

bench-fast: $(FAST_BENCH_BINARY)
	./$(FAST_BENCH_BINARY)

clean:
	rm -rf $(BUILD_DIR)
