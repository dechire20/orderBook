# orderBook

Price-time priority limit order book matching engine in C++17.

# project layout

```
include/        public headers (Order.hpp, BookFast.hpp, BookNaive.hpp)
src/            engine implementations (BookFast.cpp, BookNaive.cpp)
bindings/       pybind11 module (module.cpp)
python/         Streamlit simulator (app.py) and lobsim adapter package
tests/          dependency-free test suite
bench/          throughput benchmark + recorded results
```

## Building and testing

```sh
make test  # runs the suite against BookNaive and BookFast
```

The suite lives in [tests/test_book.cpp](tests/test_book.cpp) and has no external
dependencies. The same source is compiled twice: `build/test_book` links
[BookNaive](include/BookNaive.hpp) and `build/test_book_fast` (built with
`-DBOOK_TEST_FAST`) links [BookFast](include/BookFast.hpp). Pass name filters to
run a subset, list the test names, or adjust the per-test timeout:

```sh
./build/test_book_fast fifo modify
./build/test_book_fast --list
./build/test_book_fast --timeout 2
```

Each test runs in its own process, so a hang or a crash is reported as a failure
instead of aborting the run.

## Benchmarking

```sh
make bench
./build/bench_book --repeats 5 --scaling       # BookNaive baseline
./build/bench_book_fast --repeats 5 --scaling  # BookFast
```

Workload definitions, the test environment, and recorded results for the
current engine are in [bench/results.md](bench/results.md).

## CMake build

Requires `cmake` (`python3 -m pip install --user cmake` or `brew install cmake`).

[CMakeLists.txt](CMakeLists.txt) builds the same targets (`test_book_naive`,
`test_book_fast`, `bench_book`, `bench_book_fast`) plus the optional `bookfast`
Python module. A separate build directory keeps it clear of the Makefile's
`build/`:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake
ctest --test-dir build-cmake
```

## Python bindings

[bindings/module.cpp](bindings/module.cpp) exposes `BookFast` to Python through
pybind11:

```sh
python3 -m pip install --user pybind11
make bindings   # -> build/book*.so
```

The CMake build finds a pip-installed pybind11 on its own and skips the module
when pybind11 or the Python development headers are missing.

A runnable example lives in [python/demo.py](python/demo.py); it locates the
built module and prints a short trading session:

```sh
make demo        # builds the module, then runs python/demo.py
```

Minimal usage:

```python
import sys; sys.path.insert(0, "build")
import book   # build/book*.so

engine = book.BookFast()
engine.add(1, 100, 50, book.Side.Sell)
engine.add(2, 100, 20, book.Side.Buy)   # -> [Trade(maker_id=1, taker_id=2, price=100, qty=20)]
engine.ask_levels                       # -> {100: [1]}
engine.cancel(1)
```

## Simulator (Streamlit)

[python/app.py](python/app.py) is a single-page trading terminal for the engine:

- market overview: best bid, best ask, spread, last price, resting orders, volume
- order entry (BUY/SELL, price, quantity) — orders go straight to `BookFast`
- live order book with per-level liquidity bars, bids high→low, asks low→high
- market depth chart (cumulative quantity, mid-price marker)
- trade tape fed by the engine's `Trade` objects
- open orders with cancel and modify (a modify that crosses executes as a taker)

```sh
python3 -m pip install --user -r python/requirements.txt
make app         # builds the module, then `streamlit run python/app.py`
```

[python/lobsim/engine.py](python/lobsim/engine.py) wraps the extension: matching
is done entirely in C++, and the wrapper mirrors resting orders so the UI can
show price levels and partially filled quantities.

## Scenarios covered

| Area | What is checked |
| --- | --- |
| Full fill | A resting order that is fully consumed is removed, and its price level disappears |
| Partial fill | A partially filled resting order keeps its remaining quantity; an unfilled taker rests its remainder |
| Price priority | Best-priced resting order matches first, trades print at the maker price, and matching stops at the limit price |
| FIFO | Orders at the same price match oldest first, including a partially filled one |
| Cancellation | Removes the order, drops an emptied level, preserves FIFO of the rest, and rejects unknown/wrong-side ids |
| Modification | Changes price/quantity, crosses as a taker when it becomes marketable, ignores unknown ids and zero quantity |
| Duplicate ids | A live id is rejected on either side without executing; ids are reusable after a cancel |
| Empty book | No best prices, incoming orders rest without trading, zero-quantity orders are ignored |
| Id index (BookFast) | Cancel by id at the head, middle, or tail of a level; relocated orders remain cancellable after modify; filled maker ids are forgotten; a taker remainder is indexed |
