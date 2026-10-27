// Throughput benchmark for BookNaive (the book.hpp / book.cpp baseline:
// std::map of price levels holding std::list of orders).
//
//   make bench
//   ./build/bench_book [--repeats N]
//
// Every workload is fully deterministic: the same sizes and seeds always
// produce the same sequence of orders, so runs are comparable.

#include "book.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

// The project currently ships one engine; the plan refers to it as BookNaive.
// Aliasing here keeps the benchmark ready for faster variants later on.
using BookNaive = Book;

namespace {

using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------------- utilities

struct SplitMix64 {
    uint64_t state;

    explicit SplitMix64(uint64_t seed) : state(seed) {}

    uint64_t next() {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    uint32_t below(uint32_t bound) {
        return static_cast<uint32_t>(next() % bound);
    }
};

double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

// ---------------------------------------------------------------- workloads

struct WorkloadResult {
    double seconds = 0.0;
    uint64_t checksum = 0;
    uint64_t operations = 0;
    uint64_t trades = 0;
    uint64_t adds = 0;
    uint64_t matches = 0;
    uint64_t cancels = 0;
    uint64_t cancelMisses = 0;
};

// Resting-only inserts: bids and asks sit far apart so nothing matches.
// Measures add() plus the per-order bookkeeping it performs.
WorkloadResult workloadInsert(uint64_t count) {
    BookNaive book;
    WorkloadResult result;
    result.operations = count;

    SplitMix64 rng(0xA11CE);
    uint64_t nextId = 1;

    const auto start = Clock::now();
    for (uint64_t i = 0; i < count; ++i) {
        const bool isBid = (i % 2 == 0);
        const uint32_t price =
            isBid ? 100000 - rng.below(5000) : 200000 + rng.below(5000);
        const Order order{nextId++, price, 10,
                          isBid ? Side::Buy : Side::Sell};
        result.checksum += book.add(order).size();
        ++result.adds;
    }
    result.seconds = secondsSince(start);
    return result;
}

// Matching: an ask-only book is prefilled outside the timed region, then each
// aggressive buy exactly consumes the best resting ask (one trade per order).
WorkloadResult workloadMatch(uint64_t restingCount) {
    BookNaive book;
    WorkloadResult result;
    result.operations = restingCount;

    for (uint64_t i = 0; i < restingCount; ++i) {
        book.add(Order{i + 1, static_cast<uint32_t>(1000 + i), 10,
                       Side::Sell});
    }

    const uint32_t limit = static_cast<uint32_t>(1000 + restingCount - 1);
    uint64_t nextId = restingCount + 1;

    const auto start = Clock::now();
    for (uint64_t i = 0; i < restingCount; ++i) {
        const auto trades = book.add(Order{nextId++, limit, 10, Side::Buy});
        result.trades += trades.size();
        result.checksum += trades.size();
        ++result.adds;
        ++result.matches;
    }
    result.seconds = secondsSince(start);
    return result;
}

// Cancellation: bids spread over 100 price levels are prefilled outside the
// timed region, then cancelled in a shuffled order.
WorkloadResult workloadCancel(uint64_t count) {
    BookNaive book;
    WorkloadResult result;
    result.operations = count;

    std::vector<uint64_t> ids;
    ids.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
        book.add(Order{i + 1, static_cast<uint32_t>(1000 + (i % 100)), 10,
                       Side::Buy});
        ids.push_back(i + 1);
    }

    SplitMix64 rng(0xC0FFEE);
    for (uint64_t i = ids.size(); i > 1; --i) {
        std::swap(ids[i - 1], ids[rng.next() % i]);
    }

    const auto start = Clock::now();
    for (uint64_t id : ids) {
        if (book.cancel(Side::Buy, id)) {
            ++result.cancels;
            result.checksum += 1;
        } else {
            ++result.cancelMisses;
        }
    }
    result.seconds = secondsSince(start);
    return result;
}

// Mixed flow: resting bids, aggressive buys that consume resting asks, and
// random cancels, with the tracked bid count held inside [minBids, maxBids].
WorkloadResult workloadMixed(uint64_t count) {
    constexpr uint64_t minBids = 5000;
    constexpr uint64_t maxBids = 20000;
    constexpr uint64_t maxAsks = 20000;

    BookNaive book;
    WorkloadResult result;
    result.operations = count;

    std::vector<uint64_t> liveBids;
    liveBids.reserve(maxBids + 1);
    uint64_t asksResting = 0;

    SplitMix64 rng(0xBEEF);
    uint64_t nextId = 1;

    const auto start = Clock::now();
    for (uint64_t step = 0; step < count; ++step) {
        const uint32_t bucket = rng.below(100);

        bool doAdd = false;
        bool doAggress = false;
        bool doCancel = false;

        if (liveBids.size() > maxBids) {
            doCancel = true;
        } else if (asksResting > maxAsks) {
            doAggress = true;
        } else if (liveBids.size() < minBids) {
            doAdd = true;
        } else if (bucket < 60) {
            doAdd = true;
        } else if (bucket < 90) {
            doAggress = true;
        } else {
            doCancel = true;
        }

        if (doAggress && asksResting == 0) {
            doAggress = false;
            doAdd = true;
        }
        if (doCancel && liveBids.empty()) {
            doCancel = false;
            doAdd = true;
        }

        if (doAdd) {
            if (rng.below(2) == 0) {
                const Order order{nextId++, 1000 + rng.below(100), 10,
                                  Side::Buy};
                result.checksum += book.add(order).size();
                ++result.adds;
                liveBids.push_back(order.id);
            } else {
                const Order order{nextId++, 2000 + rng.below(100), 10,
                                  Side::Sell};
                result.checksum += book.add(order).size();
                ++result.adds;
                ++asksResting;
            }
        } else if (doAggress) {
            // Consume the best ask entirely so the incoming order never rests.
            const uint32_t price = book.asks.begin()->first;
            const uint32_t qty = book.asks.begin()->second.front().qty;
            const auto trades =
                book.add(Order{nextId++, price, qty, Side::Buy});
            result.trades += trades.size();
            result.checksum += trades.size();
            ++result.adds;
            ++result.matches;
            if (!trades.empty()) {
                --asksResting;
            }
        } else {
            const uint64_t index = rng.next() % liveBids.size();
            const uint64_t id = liveBids[index];
            liveBids[index] = liveBids.back();
            liveBids.pop_back();
            if (book.cancel(Side::Buy, id)) {
                ++result.cancels;
                result.checksum += 1;
            } else {
                ++result.cancelMisses;
            }
        }
    }
    result.seconds = secondsSince(start);
    return result;
}

// ---------------------------------------------------------------- reporting

struct Scenario {
    std::string name;
    uint64_t operations = 0;
    uint64_t trades = 0;
    uint64_t adds = 0;
    uint64_t matches = 0;
    uint64_t cancels = 0;
    uint64_t cancelMisses = 0;
    double medianSeconds = 0.0;
    double bestSeconds = 0.0;
    double worstSeconds = 0.0;
    uint64_t checksum = 0;
};

std::string formatRate(double perSecond) {
    char buffer[64];
    if (perSecond >= 1e6) {
        std::snprintf(buffer, sizeof(buffer), "%.2fM", perSecond / 1e6);
    } else if (perSecond >= 1e3) {
        std::snprintf(buffer, sizeof(buffer), "%.1fk", perSecond / 1e3);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.0f", perSecond);
    }
    return buffer;
}

Scenario runScenario(const std::string &name,
                     const std::function<WorkloadResult()> &workload,
                     std::size_t repeats) {
    workload(); // warmup: caches and allocator, result discarded

    std::vector<double> samples;
    samples.reserve(repeats);

    Scenario scenario;
    scenario.name = name;

    for (std::size_t i = 0; i < repeats; ++i) {
        const WorkloadResult result = workload();
        samples.push_back(result.seconds);
        scenario.operations = result.operations;
        scenario.trades = result.trades;
        scenario.adds = result.adds;
        scenario.matches = result.matches;
        scenario.cancels = result.cancels;
        scenario.cancelMisses = result.cancelMisses;
        scenario.checksum = result.checksum;
    }

    std::sort(samples.begin(), samples.end());
    scenario.bestSeconds = samples.front();
    scenario.medianSeconds = samples[samples.size() / 2];
    scenario.worstSeconds = samples.back();
    return scenario;
}

} // namespace

int main(int argc, char **argv) {
    std::size_t repeats = 3;
    bool scaling = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--repeats" && i + 1 < argc) {
            repeats = static_cast<std::size_t>(std::atol(argv[++i]));
        } else if (argument == "--scaling") {
            scaling = true;
        } else if (argument == "--help" || argument == "-h") {
            std::printf("usage: %s [--repeats N] [--scaling]\n",
                        argv[0]);
            return 0;
        } else {
            std::printf("unknown argument: %s\n", argument.c_str());
            return 2;
        }
    }
    if (repeats == 0) {
        repeats = 1;
    }

    constexpr uint64_t insertOrders = 20000;
    constexpr uint64_t restingForMatch = 20000;
    constexpr uint64_t cancelOrders = 10000;
    constexpr uint64_t mixedOperations = 50000;

    std::printf("BookNaive throughput benchmark\n");
    std::printf("engine:      Book (std::map<uint32_t, std::list<Order>>)\n");
    std::printf("repeats:     %zu measured + 1 warmup\n", repeats);
    std::printf("workloads:   insert=%llu resting, match=%llu resting + "
                "%llu aggressive, cancel=%llu, mixed=%llu ops\n\n",
                static_cast<unsigned long long>(insertOrders),
                static_cast<unsigned long long>(restingForMatch),
                static_cast<unsigned long long>(restingForMatch),
                static_cast<unsigned long long>(cancelOrders),
                static_cast<unsigned long long>(mixedOperations));

    std::vector<Scenario> scenarios;
    scenarios.push_back(runScenario(
        "insert (rest only)",
        [] { return workloadInsert(insertOrders); }, repeats));
    scenarios.push_back(runScenario(
        "match (1 trade per order)",
        [] { return workloadMatch(restingForMatch); }, repeats));
    scenarios.push_back(runScenario(
        "cancel",
        [] { return workloadCancel(cancelOrders); }, repeats));
    scenarios.push_back(runScenario(
        "mixed (insert/match/cancel)",
        [] { return workloadMixed(mixedOperations); }, repeats));

    std::printf("%-30s %8s %8s %11s %11s %10s\n", "scenario", "ops", "trades",
                "median ops/s", "best ops/s", "median ms");
    std::printf("%-30s %8s %8s %11s %11s %10s\n",
                "------------------------------", "--------", "--------",
                "-----------", "-----------", "----------");

    uint64_t checksum = 0;
    for (const Scenario &scenario : scenarios) {
        const double medianRate =
            static_cast<double>(scenario.operations) / scenario.medianSeconds;
        const double bestRate =
            static_cast<double>(scenario.operations) / scenario.bestSeconds;
        std::printf("%-30s %8llu %8llu %11s %11s %10.1f\n",
                    scenario.name.c_str(),
                    static_cast<unsigned long long>(scenario.operations),
                    static_cast<unsigned long long>(scenario.trades),
                    formatRate(medianRate).c_str(),
                    formatRate(bestRate).c_str(),
                    scenario.medianSeconds * 1000.0);
        checksum ^= scenario.checksum;
    }

    std::printf("\noperation mix (timed region):\n");
    std::printf("  %-30s %9s %9s %9s %9s\n", "scenario", "adds", "matches",
                "cancels", "missed");
    for (const Scenario &scenario : scenarios) {
        std::printf("  %-30s %9llu %9llu %9llu %9llu\n",
                    scenario.name.c_str(),
                    static_cast<unsigned long long>(scenario.adds),
                    static_cast<unsigned long long>(scenario.matches),
                    static_cast<unsigned long long>(scenario.cancels),
                    static_cast<unsigned long long>(scenario.cancelMisses));
    }

    std::printf("\nper-scenario spread (min..max ms):\n");
    for (const Scenario &scenario : scenarios) {
        std::printf("  %-30s %8.1f .. %.1f\n", scenario.name.c_str(),
                    scenario.bestSeconds * 1000.0,
                    scenario.worstSeconds * 1000.0);
    }

    std::printf("\nchecksum: 0x%llX (stable across runs = repeatable workload)\n",
                static_cast<unsigned long long>(checksum));

    if (scaling) {
        std::printf("\ninsert throughput vs book depth (rest-only):\n");
        std::printf("  %8s %11s %11s %10s\n", "orders", "median ms",
                    "median ops/s", "ns/op");
        for (uint64_t orders : {2000ULL, 5000ULL, 10000ULL, 20000ULL}) {
            const Scenario scenario = runScenario(
                "insert", [orders] { return workloadInsert(orders); }, repeats);
            const double rate =
                static_cast<double>(scenario.operations) / scenario.medianSeconds;
            std::printf("  %8llu %11.1f %11s %10.0f\n",
                        static_cast<unsigned long long>(orders),
                        scenario.medianSeconds * 1000.0, formatRate(rate).c_str(),
                        scenario.medianSeconds * 1e9 / scenario.operations);
        }
    }
    return 0;
}
