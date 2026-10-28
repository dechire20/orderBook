# BookNaive benchmark results

Baseline throughput of `BookNaive`, the engine currently in
[BookNaive.hpp](../BookNaive.hpp) / [BookNaive.cpp](../BookNaive.cpp): a
`std::map<uint32_t, std::list<Order>>` per side. The benchmark source is
[bench/bench.cpp](bench.cpp) and drives the engine through `BookNaive`'s public
interface, so a faster engine can be compared without touching the harness.

## Environment and build

| Item | Value |
| --- | --- |
| Machine | MacBook (Apple M1), 8 cores, 16 GB RAM |
| OS | macOS 27.0.1 (build 26A434), arm64 |
| Compiler | Apple clang 21.0.0 (clang-2100.3.34.2), target `arm64-apple-darwin27.0.0` |
| Build command | `make bench` |
| Flags | `-std=c++17 -O2 -g -Wall -Wextra -Wpedantic` |
| Mode | Single-threaded, optimized (`-O2`, no LTO, no PGO) |
| Timer | `std::chrono::steady_clock` |
| Measurement | 1 warmup run (discarded) + 5 measured runs, median reported |
| Run date | 2026-10-27 |

## Workloads

All workloads are deterministic: fixed sizes and seeded `SplitMix64` order
streams, so every run submits the identical sequence of requests. This is
verified by the checksum the benchmark prints — it was `0x57E3` on every run.
Book prefill is done outside the timed region for the match and cancel
scenarios; only the operation loop is measured.

| Scenario | Timed operations | Book state | What it measures |
| --- | --- | --- | --- |
| `insert (rest only)` | 20,000 adds | grows 0 → 20,000 resting orders | Adds that never cross (bids near 100k, asks near 200k) |
| `match (1 trade per order)` | 20,000 aggressive adds | prefilled with 20,000 asks, drains to 0 | Price-priority matching, exactly one trade per order |
| `cancel` | 10,000 cancels | 10,000 bids over 100 price levels, cancelled in shuffled order | Cancellation, including level removal |
| `mixed (insert/match/cancel)` | 50,000 operations | depth clamped to 5,000 – 20,000 bids / 20,000 asks | Realistic mix: resting adds, aggressive buys consuming the best ask, random cancels |

Mixed workload weights: 60 % resting add, 30 % aggressive match, 10 % cancel,
with depth clamps that force a cancel when bids exceed 20,000 and a match when
asks exceed 20,000.

## Results

| Scenario | Ops | Trades | Median time | Median ops/s | Best ops/s | ns/op | Trades/s |
| --- | --- | --- | --- | --- | --- | --- | --- |
| insert (rest only) | 20,000 | 0 | 1,986.3 ms | 10.1k | 10.2k | 99,315 | — |
| match (1 trade per order) | 20,000 | 20,000 | 1,224.6 ms | 16.3k | 16.6k | 61,230 | 16.3k |
| cancel | 10,000 | 0 | 188.4 ms | 53.1k | 53.5k | 18,840 | — |
| mixed (insert/match/cancel) | 50,000 | 12,067 | 4,831.8 ms | 10.3k | 10.4k | 96,636 | 2.5k |

Within a single run the measured spread was under 3 % (e.g. insert
1,956 – 1,998 ms, mixed 4,817 – 4,851 ms).

### Operation mix actually executed

| Scenario | Adds | Matching adds | Cancels | Failed cancels |
| --- | --- | --- | --- | --- |
| insert (rest only) | 20,000 | 0 | 0 | 0 |
| match (1 trade per order) | 20,000 | 20,000 | 0 | 0 |
| cancel | 0 | 0 | 10,000 | 0 |
| mixed (insert/match/cancel) | 45,984 | 12,067 | 4,016 | 0 |

## Scaling: insert throughput vs book depth

`./build/bench_book --repeats 5 --scaling`

| Resting orders | Median time | Median ops/s | ns/op |
| --- | --- | --- | --- |
| 2,000 | 13.8 ms | 144.8k | 6,908 |
| 5,000 | 115.0 ms | 43.5k | 22,998 |
| 10,000 | 446.1 ms | 22.4k | 44,612 |
| 20,000 | 1,986.9 ms | 10.1k | 99,346 |

Cost per insert roughly doubles every time the book doubles, so `add` is
**O(n)** in the number of resting orders and the insert loop as a whole is
**O(n²)**.

## Analysis

- **`add` is the bottleneck.** `Book::add` calls `containsOrderId`, which scans
  every resting order on both sides before accepting an order. That single
  duplicate-ID check dominates every insert (≈ 99 µs/op at 20,000 resting
  orders) and it is also the bulk of the 61 µs/op spent on each matching
  aggressive order — the actual price-level lookup and `std::list` front
  access are far cheaper.
- **Cancel is comparatively cheap here**, because it walks from the best price
  outward and the cancel workload spreads 10,000 orders over only 100 levels
  (≈ 18.8 µs/op). Its cost still grows with book depth and is a linear scan per
  call.
- **Matching itself is not the limiter yet.** The engine produces one trade per
  aggressive order at 16.3k trades/s, but that figure is capped by the O(n)
  ID scan rather than by the matching logic.
- **Mixed flow sustains ~10.3k ops/s** at a 5k – 20k order book, i.e. the
  realistic case is dominated by the same per-add ID scan.

These numbers are the baseline to compare future engines against; re-record
this file after changing the data structures so the comparison stays honest.

## Reproducing

```sh
make bench                                # default: 3 repeats, full workload set
./build/bench_book --repeats 5 --scaling  # more repeats + insert-vs-depth table
```

The benchmark prints a checksum; it must match `0x57E3` for a genuinely
comparable run.

## Caveats

- Measurements were taken on a shared, unpinned machine, so absolute numbers
  drift across sessions: insert medians ranged from 8.4k to 10.9k ops/s
  (≈ ±15 %) between runs, while the ordering of the scenarios never changed.
- Timings include `std::list` node allocation, which is part of the engine's
  real cost, plus `-g` in the flags (no measurable effect at `-O2`).
- Single-threaded only; the engine has no concurrency support yet.
