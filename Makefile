CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -Wpedantic
INCLUDES := -Iinclude -I.
BUILD_DIR := build

ifeq ($(origin CXX), default)
CXX := clang++
endif

PYTHON ?= python3

ifeq ($(shell uname -s),Darwin)
PY_LDFLAGS := -undefined dynamic_lookup
endif

ENGINE_SOURCES := src/BookNaive.cpp src/BookFast.cpp
ENGINE_HEADERS := include/BookNaive.hpp include/BookFast.hpp include/Order.hpp
TEST_SOURCES := tests/test_book.cpp $(ENGINE_SOURCES)

TEST_BINARY := $(BUILD_DIR)/test_book
FAST_TEST_BINARY := $(BUILD_DIR)/test_book_fast

BENCH_SOURCES := bench/bench.cpp $(ENGINE_SOURCES)
BENCH_BINARY := $(BUILD_DIR)/bench_book
FAST_BENCH_BINARY := $(BUILD_DIR)/bench_book_fast

BINDING_SOURCES := bindings/module.cpp $(ENGINE_SOURCES)

.PHONY: all test test-naive test-fast bench bench-naive bench-fast bindings app demo clean

all: test

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(TEST_BINARY): $(TEST_SOURCES) $(ENGINE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(TEST_SOURCES)

$(FAST_TEST_BINARY): $(TEST_SOURCES) $(ENGINE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DBOOK_TEST_FAST -o $@ $(TEST_SOURCES)

test: test-naive test-fast

test-naive: $(TEST_BINARY)
	./$(TEST_BINARY)

test-fast: $(FAST_TEST_BINARY)
	./$(FAST_TEST_BINARY)

$(BENCH_BINARY): $(BENCH_SOURCES) $(ENGINE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(BENCH_SOURCES)

$(FAST_BENCH_BINARY): $(BENCH_SOURCES) $(ENGINE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DBENCH_BOOK_FAST -o $@ $(BENCH_SOURCES)

bench: bench-naive bench-fast

bench-naive: $(BENCH_BINARY)
	./$(BENCH_BINARY)

bench-fast: $(FAST_BENCH_BINARY)
	./$(FAST_BENCH_BINARY)

# Optional pybind11 extension; requires `python3 -m pip install pybind11`.
bindings: $(BINDING_SOURCES) $(ENGINE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -fPIC $(PY_LDFLAGS) \
		$$($(PYTHON) -m pybind11 --includes) \
		-o $(BUILD_DIR)/book$$($(PYTHON) -c 'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX"))') \
		$(BINDING_SOURCES)

# Streamlit simulator (needs `python3 -m pip install -r python/requirements.txt`).
app: $(BINDING_SOURCES) $(ENGINE_HEADERS) | $(BUILD_DIR)
	@$(MAKE) --no-print-directory bindings
	$(PYTHON) -m streamlit run python/app.py

demo: $(BINDING_SOURCES) $(ENGINE_HEADERS) | $(BUILD_DIR)
	@$(MAKE) --no-print-directory bindings
	$(PYTHON) python/demo.py

clean:
	rm -rf $(BUILD_DIR)
	find python -name __pycache__ -type d -prune -exec rm -rf {} +
