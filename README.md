# orderBook

Price-time priority limit order book matching engine in C++17.

## Building and testing

```sh
make test  # runs the suite against BookNaive and BookFast
```

The suite lives in [tests/test_book.cpp](tests/test_book.cpp) and has no external
dependencies. The same source is compiled twice: `build/test_book` links
[BookNaive](BookNaive.hpp) and `build/test_book_fast` (built with
`-DBOOK_TEST_FAST`) links [BookFast](BookFast.hpp). Pass name filters to run a
subset, list the test names, or adjust the per-test timeout:

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
