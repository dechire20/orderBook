# orderBook

Price-time priority limit order book matching engine in C++17.

## Building and testing

```sh
make test
```

The suite lives in [tests/test_book.cpp](tests/test_book.cpp) and has no external
dependencies. Pass name filters to run a subset, list the test names, or adjust
the per-test timeout:

```sh
./build/test_book fifo modify
./build/test_book --list
./build/test_book --timeout 2
```

Each test runs in its own process, so a hang or a crash is reported as a failure
instead of aborting the run.

## Benchmarking

```sh
make bench
./build/bench_book --repeats 5 --scaling
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
