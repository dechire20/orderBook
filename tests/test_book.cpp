// Dependency-free test suite for the order book matching engine.
//
//   make test                      build and run every test
//   ./build/test_book fifo cancel  run only tests whose name contains a filter
//   ./build/test_book --list       list test names
//   ./build/test_book --timeout 5  per-test timeout in seconds (default 10)

#include "../book.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#define TEST_RUN_ISOLATED 1
#endif

namespace test {

class Context {
  public:
    void fail(const char *file, int line, const std::string &message) {
        std::ostringstream out;
        out << file << ':' << line << ": " << message;
        failures_.push_back(out.str());
    }

    const std::vector<std::string> &failures() const { return failures_; }

  private:
    std::vector<std::string> failures_;
};

using TestFn = void (*)(Context &);

struct TestCase {
    std::string name;
    TestFn fn;
};

inline std::vector<TestCase> &allTests() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char *name, TestFn fn) { allTests().push_back({name, fn}); }
};

// ---------------------------------------------------------------- formatting

inline std::string show(Side side) {
    return side == Side::Buy ? "Buy" : "Sell";
}

inline std::string show(std::nullopt_t) { return "nullopt"; }

inline std::string show(const std::optional<uint32_t> &value) {
    if (!value) {
        return "nullopt";
    }
    return std::to_string(*value);
}

inline std::string show(const Trade &trade) {
    std::ostringstream out;
    out << "{maker=" << trade.maker_id << ", taker=" << trade.taker_id
        << ", price=" << trade.price << ", qty=" << trade.qty << '}';
    return out.str();
}

inline std::string show(const std::vector<Trade> &trades) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < trades.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << show(trades[i]);
    }
    out << ']';
    return out.str();
}

inline std::string show(const std::vector<uint64_t> &ids) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << ids[i];
    }
    out << ']';
    return out.str();
}

inline std::string show(const std::vector<uint32_t> &values) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << values[i];
    }
    out << ']';
    return out.str();
}

template <typename T> std::string show(const T &value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

} // namespace test

// ------------------------------------------------------------------- macros

#define TEST(name)                                                             \
    static void name(::test::Context & t);                                     \
    static ::test::Registrar name##_registrar(#name, name);                     \
    static void name(::test::Context &t)

#define CHECK_TRUE(cond)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            t.fail(__FILE__, __LINE__, "expected true: " #cond);               \
        }                                                                      \
    } while (false)

#define CHECK_FALSE(cond)                                                      \
    do {                                                                       \
        if ((cond)) {                                                          \
            t.fail(__FILE__, __LINE__, "expected false: " #cond);              \
        }                                                                      \
    } while (false)

#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        const auto &actualValue = (actual);                                    \
        const auto &expectedValue = (expected);                                \
        if (!(actualValue == expectedValue)) {                                 \
            t.fail(__FILE__, __LINE__,                                         \
                   std::string("expected " #actual " == " #expected " but got ") + \
                       ::test::show(actualValue) + " vs " +                    \
                       ::test::show(expectedValue));                           \
        }                                                                      \
    } while (false)

// Trade has no operator==, so trades are compared through their printed form.
#define CHECK_TRADES(actual, expected)                                         \
    do {                                                                       \
        const auto &actualValue = (actual);                                    \
        const auto &expectedValue = (expected);                                \
        if (::test::show(actualValue) != ::test::show(expectedValue)) {        \
            t.fail(__FILE__, __LINE__,                                         \
                   std::string("unexpected trades for " #actual ": got ") +    \
                       ::test::show(actualValue) + ", want " +                 \
                       ::test::show(expectedValue));                           \
        }                                                                      \
    } while (false)

// ------------------------------------------------------------------ helpers

namespace {

using BookSide = std::map<uint32_t, std::list<Order>>;

const std::vector<Trade> NO_TRADES{};

Order makeOrder(uint64_t id, Side side, uint32_t price, uint32_t qty) {
    return Order{id, price, qty, side};
}

Trade makeTrade(uint64_t makerId, uint64_t takerId, uint32_t price,
                uint32_t qty) {
    return Trade{makerId, takerId, price, qty};
}

const BookSide &levels(const Book &book, Side side) {
    return side == Side::Buy ? book.bids : book.asks;
}

std::vector<uint64_t> idsAt(const Book &book, Side side, uint32_t price) {
    std::vector<uint64_t> ids;
    const BookSide &sideLevels = levels(book, side);
    auto level = sideLevels.find(price);
    if (level == sideLevels.end()) {
        return ids;
    }
    for (const Order &order : level->second) {
        ids.push_back(order.id);
    }
    return ids;
}

std::vector<uint32_t> quantitiesAt(const Book &book, Side side,
                                   uint32_t price) {
    std::vector<uint32_t> quantities;
    const BookSide &sideLevels = levels(book, side);
    auto level = sideLevels.find(price);
    if (level == sideLevels.end()) {
        return quantities;
    }
    for (const Order &order : level->second) {
        quantities.push_back(order.qty);
    }
    return quantities;
}

std::size_t restingCount(const Book &book, Side side) {
    std::size_t count = 0;
    for (const auto &level : levels(book, side)) {
        count += level.second.size();
    }
    return count;
}

} // namespace

// -------------------------------------------------------------------- tests
//
// Scenario 1: full fill

TEST(full_fill_removes_fully_consumed_resting_ask) {
    Book book;
    CHECK_TRADES(book.add(makeOrder(1, Side::Sell, 100, 50)), NO_TRADES);

    auto trades = book.add(makeOrder(2, Side::Buy, 100, 50));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 2, 100, 50)}));
    CHECK_TRUE(book.asks.empty());
    CHECK_TRUE(book.bids.empty());
    CHECK_EQ(book.best_ask(), std::nullopt);
    CHECK_EQ(book.best_bid(), std::nullopt);
}

TEST(full_fill_removes_fully_consumed_resting_bid) {
    Book book;
    CHECK_TRADES(book.add(makeOrder(1, Side::Buy, 100, 50)), NO_TRADES);

    auto trades = book.add(makeOrder(2, Side::Sell, 100, 50));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 2, 100, 50)}));
    CHECK_TRUE(book.bids.empty());
    CHECK_TRUE(book.asks.empty());
}

TEST(full_fill_only_removes_the_touched_order) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));
    book.add(makeOrder(2, Side::Sell, 100, 10));
    book.add(makeOrder(3, Side::Sell, 105, 10));

    auto trades = book.add(makeOrder(4, Side::Buy, 100, 10));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 4, 100, 10)}));
    CHECK_EQ(idsAt(book, Side::Sell, 100), (std::vector<uint64_t>{2}));
    CHECK_EQ(idsAt(book, Side::Sell, 105), (std::vector<uint64_t>{3}));
}

// Scenario 2: partial fill

TEST(partial_fill_keeps_remainder_on_resting_ask) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 50));

    auto trades = book.add(makeOrder(2, Side::Buy, 100, 20));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 2, 100, 20)}));
    CHECK_EQ(idsAt(book, Side::Sell, 100), (std::vector<uint64_t>{1}));
    CHECK_EQ(quantitiesAt(book, Side::Sell, 100), (std::vector<uint32_t>{30}));
    CHECK_TRUE(book.bids.empty());
    CHECK_EQ(book.best_ask(), std::optional<uint32_t>{100});
}

TEST(partial_fill_keeps_remainder_on_resting_bid) {
    Book book;
    book.add(makeOrder(1, Side::Buy, 100, 50));

    auto trades = book.add(makeOrder(2, Side::Sell, 100, 20));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 2, 100, 20)}));
    CHECK_EQ(quantitiesAt(book, Side::Buy, 100), (std::vector<uint32_t>{30}));
    CHECK_TRUE(book.asks.empty());
}

TEST(partial_fill_rests_the_unfilled_taker_quantity) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 20));

    auto trades = book.add(makeOrder(2, Side::Buy, 100, 50));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 2, 100, 20)}));
    CHECK_TRUE(book.asks.empty());
    CHECK_EQ(idsAt(book, Side::Buy, 100), (std::vector<uint64_t>{2}));
    CHECK_EQ(quantitiesAt(book, Side::Buy, 100), (std::vector<uint32_t>{30}));
    CHECK_EQ(book.best_bid(), std::optional<uint32_t>{100});
}

// Scenario 3: price priority

TEST(price_priority_matches_best_ask_first) {
    Book book;
    book.add(makeOrder(3, Side::Sell, 102, 10));
    book.add(makeOrder(1, Side::Sell, 101, 10));
    book.add(makeOrder(2, Side::Sell, 100, 10));

    auto trades = book.add(makeOrder(4, Side::Buy, 102, 15));

    // Best price first, and every trade prints at the resting order's price.
    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(2, 4, 100, 10),
                                             makeTrade(1, 4, 101, 5)}));
    CHECK_EQ(idsAt(book, Side::Sell, 101), (std::vector<uint64_t>{1}));
    CHECK_EQ(quantitiesAt(book, Side::Sell, 101), (std::vector<uint32_t>{5}));
    CHECK_EQ(idsAt(book, Side::Sell, 102), (std::vector<uint64_t>{3}));
    CHECK_EQ(book.best_ask(), std::optional<uint32_t>{101});
}

TEST(price_priority_matches_best_bid_first) {
    Book book;
    book.add(makeOrder(1, Side::Buy, 100, 10));
    book.add(makeOrder(2, Side::Buy, 102, 10));
    book.add(makeOrder(3, Side::Buy, 101, 10));

    auto trades = book.add(makeOrder(4, Side::Sell, 99, 25));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(2, 4, 102, 10),
                                             makeTrade(3, 4, 101, 10),
                                             makeTrade(1, 4, 100, 5)}));
    CHECK_EQ(idsAt(book, Side::Buy, 100), (std::vector<uint64_t>{1}));
    CHECK_EQ(quantitiesAt(book, Side::Buy, 100), (std::vector<uint32_t>{5}));
    CHECK_EQ(restingCount(book, Side::Buy), std::size_t{1});
    CHECK_EQ(book.best_bid(), std::optional<uint32_t>{100});
}

TEST(price_priority_stops_at_the_limit_price) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));
    book.add(makeOrder(2, Side::Sell, 101, 10));
    book.add(makeOrder(3, Side::Sell, 102, 10));

    auto trades = book.add(makeOrder(4, Side::Buy, 100, 30));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 4, 100, 10)}));
    // The unfilled remainder rests at the taker's limit, behind the existing
    // level at the same price.
    CHECK_EQ(idsAt(book, Side::Buy, 100), (std::vector<uint64_t>{4}));
    CHECK_EQ(quantitiesAt(book, Side::Buy, 100), (std::vector<uint32_t>{20}));
    CHECK_EQ(restingCount(book, Side::Sell), std::size_t{2});
}

// FIFO at the same price

TEST(fifo_matches_oldest_order_at_the_same_price) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));
    book.add(makeOrder(2, Side::Sell, 100, 10));
    book.add(makeOrder(3, Side::Sell, 100, 10));

    auto trades = book.add(makeOrder(4, Side::Buy, 100, 20));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 4, 100, 10),
                                             makeTrade(2, 4, 100, 10)}));
    CHECK_EQ(idsAt(book, Side::Sell, 100), (std::vector<uint64_t>{3}));
}

TEST(fifo_keeps_partially_filled_order_in_front) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));
    book.add(makeOrder(2, Side::Sell, 100, 10));
    book.add(makeOrder(3, Side::Sell, 100, 10));

    auto trades = book.add(makeOrder(4, Side::Buy, 100, 15));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 4, 100, 10),
                                             makeTrade(2, 4, 100, 5)}));
    CHECK_EQ(idsAt(book, Side::Sell, 100), (std::vector<uint64_t>{2, 3}));
    CHECK_EQ(quantitiesAt(book, Side::Sell, 100),
             (std::vector<uint32_t>{5, 10}));
}

TEST(fifo_applies_to_bids_at_the_same_price) {
    Book book;
    book.add(makeOrder(1, Side::Buy, 100, 10));
    book.add(makeOrder(2, Side::Buy, 100, 10));

    auto trades = book.add(makeOrder(3, Side::Sell, 100, 15));

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 3, 100, 10),
                                             makeTrade(2, 3, 100, 5)}));
    CHECK_EQ(idsAt(book, Side::Buy, 100), (std::vector<uint64_t>{2}));
    CHECK_EQ(quantitiesAt(book, Side::Buy, 100), (std::vector<uint32_t>{5}));
}

// Cancellation

TEST(cancel_removes_resting_order) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));

    CHECK_TRUE(book.cancel(Side::Sell, 1));
    CHECK_TRUE(book.asks.empty());
    CHECK_FALSE(book.containsOrderId(1));
}

TEST(cancel_removes_level_when_last_order_leaves) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));
    book.add(makeOrder(2, Side::Sell, 105, 10));

    CHECK_TRUE(book.cancel(Side::Sell, 1));
    CHECK_EQ(book.asks.size(), std::size_t{1});
    CHECK_EQ(book.best_ask(), std::optional<uint32_t>{105});
}

TEST(cancel_middle_order_preserves_fifo_of_the_rest) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));
    book.add(makeOrder(2, Side::Sell, 100, 10));
    book.add(makeOrder(3, Side::Sell, 100, 10));

    CHECK_TRUE(book.cancel(Side::Sell, 2));
    CHECK_EQ(idsAt(book, Side::Sell, 100), (std::vector<uint64_t>{1, 3}));

    auto trades = book.add(makeOrder(4, Side::Buy, 100, 10));
    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 4, 100, 10)}));
}

TEST(cancel_removes_bid_and_keeps_level_bookkeeping) {
    Book book;
    book.add(makeOrder(1, Side::Buy, 100, 10));
    book.add(makeOrder(2, Side::Buy, 100, 10));
    book.add(makeOrder(3, Side::Buy, 99, 10));

    CHECK_TRUE(book.cancel(Side::Buy, 1));
    CHECK_EQ(idsAt(book, Side::Buy, 100), (std::vector<uint64_t>{2}));
    CHECK_TRUE(book.cancel(Side::Buy, 2));
    CHECK_EQ(book.best_bid(), std::optional<uint32_t>{99});

    auto trades = book.add(makeOrder(4, Side::Sell, 99, 10));
    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(3, 4, 99, 10)}));
}

TEST(cancel_unknown_order_returns_false) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));

    CHECK_FALSE(book.cancel(Side::Sell, 999));
    CHECK_EQ(restingCount(book, Side::Sell), std::size_t{1});
}

TEST(cancel_on_wrong_side_returns_false) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));

    CHECK_FALSE(book.cancel(Side::Buy, 1));
    CHECK_EQ(restingCount(book, Side::Sell), std::size_t{1});
}

TEST(cancel_on_empty_book_returns_false) {
    Book book;

    CHECK_FALSE(book.cancel(Side::Sell, 1));
    CHECK_FALSE(book.cancel(Side::Buy, 1));
}

// Modification

TEST(modify_changes_quantity_in_place) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 50));

    auto trades = book.modify(Side::Sell, 1, 100, 30);

    CHECK_TRADES(trades, NO_TRADES);
    CHECK_EQ(idsAt(book, Side::Sell, 100), (std::vector<uint64_t>{1}));
    CHECK_EQ(quantitiesAt(book, Side::Sell, 100), (std::vector<uint32_t>{30}));
}

TEST(modify_moves_order_to_new_price) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 50));

    auto trades = book.modify(Side::Sell, 1, 105, 50);

    CHECK_TRADES(trades, NO_TRADES);
    CHECK_EQ(book.asks.count(100), std::size_t{0});
    CHECK_EQ(idsAt(book, Side::Sell, 105), (std::vector<uint64_t>{1}));
    CHECK_EQ(quantitiesAt(book, Side::Sell, 105), (std::vector<uint32_t>{50}));
    CHECK_EQ(book.best_ask(), std::optional<uint32_t>{105});
}

TEST(modify_into_the_book_executes_as_a_taker) {
    Book book;
    book.add(makeOrder(1, Side::Buy, 100, 20));
    book.add(makeOrder(2, Side::Sell, 110, 20));

    auto trades = book.modify(Side::Sell, 2, 100, 20);

    CHECK_TRADES(trades, (std::vector<Trade>{makeTrade(1, 2, 100, 20)}));
    CHECK_TRUE(book.bids.empty());
    CHECK_TRUE(book.asks.empty());
}

TEST(modify_unknown_order_is_ignored) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));

    auto trades = book.modify(Side::Sell, 999, 101, 10);

    CHECK_TRADES(trades, NO_TRADES);
    CHECK_EQ(idsAt(book, Side::Sell, 100), (std::vector<uint64_t>{1}));
    CHECK_EQ(book.asks.count(101), std::size_t{0});
}

TEST(modify_to_zero_quantity_is_rejected) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));

    auto trades = book.modify(Side::Sell, 1, 100, 0);

    CHECK_TRADES(trades, NO_TRADES);
    CHECK_EQ(quantitiesAt(book, Side::Sell, 100), (std::vector<uint32_t>{10}));
}

// Duplicate order ids

TEST(duplicate_id_on_same_side_is_rejected) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));

    auto trades = book.add(makeOrder(1, Side::Sell, 101, 10));

    CHECK_TRADES(trades, NO_TRADES);
    CHECK_EQ(restingCount(book, Side::Sell), std::size_t{1});
    CHECK_EQ(idsAt(book, Side::Sell, 100), (std::vector<uint64_t>{1}));
    CHECK_EQ(quantitiesAt(book, Side::Sell, 100),
             (std::vector<uint32_t>{10}));
    CHECK_EQ(book.asks.count(101), std::size_t{0});
}

TEST(duplicate_id_across_sides_is_rejected) {
    Book book;
    book.add(makeOrder(1, Side::Buy, 99, 10));

    auto trades = book.add(makeOrder(1, Side::Sell, 101, 10));

    CHECK_TRADES(trades, NO_TRADES);
    CHECK_TRUE(book.asks.empty());
    CHECK_EQ(idsAt(book, Side::Buy, 99), (std::vector<uint64_t>{1}));
}

TEST(duplicate_id_never_executes_against_the_book) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));

    auto trades = book.add(makeOrder(1, Side::Buy, 100, 10));

    CHECK_TRADES(trades, NO_TRADES);
    CHECK_EQ(idsAt(book, Side::Sell, 100), (std::vector<uint64_t>{1}));
    CHECK_EQ(quantitiesAt(book, Side::Sell, 100),
             (std::vector<uint32_t>{10}));
    CHECK_TRUE(book.bids.empty());
}

TEST(id_becomes_reusable_after_cancel) {
    Book book;
    book.add(makeOrder(1, Side::Sell, 100, 10));
    CHECK_TRUE(book.cancel(Side::Sell, 1));

    auto trades = book.add(makeOrder(1, Side::Sell, 100, 25));

    CHECK_TRADES(trades, NO_TRADES);
    CHECK_EQ(quantitiesAt(book, Side::Sell, 100), (std::vector<uint32_t>{25}));
}

// Empty book behaviour

TEST(empty_book_has_no_best_prices) {
    Book book;

    CHECK_EQ(book.best_bid(), std::nullopt);
    CHECK_EQ(book.best_ask(), std::nullopt);
    CHECK_TRUE(book.bids.empty());
    CHECK_TRUE(book.asks.empty());
    CHECK_FALSE(book.containsOrderId(1));
}

TEST(empty_book_rests_incoming_orders_without_trading) {
    Book book;

    CHECK_TRADES(book.add(makeOrder(1, Side::Buy, 100, 10)), NO_TRADES);
    CHECK_EQ(book.best_bid(), std::optional<uint32_t>{100});
    CHECK_EQ(book.best_ask(), std::nullopt);

    CHECK_TRADES(book.add(makeOrder(2, Side::Sell, 105, 10)), NO_TRADES);
    CHECK_EQ(book.best_ask(), std::optional<uint32_t>{105});
    CHECK_EQ(book.best_bid(), std::optional<uint32_t>{100});
    CHECK_EQ(restingCount(book, Side::Buy), std::size_t{1});
    CHECK_EQ(restingCount(book, Side::Sell), std::size_t{1});
}

TEST(non_crossing_orders_rest_without_trading) {
    Book book;

    CHECK_TRADES(book.add(makeOrder(1, Side::Buy, 99, 10)), NO_TRADES);
    CHECK_TRADES(book.add(makeOrder(2, Side::Sell, 100, 10)), NO_TRADES);

    CHECK_EQ(book.best_bid(), std::optional<uint32_t>{99});
    CHECK_EQ(book.best_ask(), std::optional<uint32_t>{100});
}

TEST(zero_quantity_orders_are_ignored) {
    Book book;

    CHECK_TRADES(book.add(makeOrder(1, Side::Buy, 100, 0)), NO_TRADES);
    CHECK_TRADES(book.add(makeOrder(2, Side::Sell, 100, 0)), NO_TRADES);

    CHECK_TRUE(book.bids.empty());
    CHECK_TRUE(book.asks.empty());
    CHECK_EQ(book.best_bid(), std::nullopt);
    CHECK_EQ(book.best_ask(), std::nullopt);
}

// ------------------------------------------------------------------- runner

namespace {

// Shared by the per-test process and the parent that supervises it.
void report(const test::TestCase &testCase, test::Context &context) {
    if (context.failures().empty()) {
        std::printf("PASS  %s\n", testCase.name.c_str());
    } else {
        std::printf("FAIL  %s\n", testCase.name.c_str());
        for (const std::string &failure : context.failures()) {
            std::printf("        %s\n", failure.c_str());
        }
    }
    std::fflush(stdout);
}

} // namespace

int main(int argc, char **argv) {
    // Unbuffered so a forked child never re-flushes its parent's pending output.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::vector<std::string> filters;
    bool listOnly = false;
    int timeoutSeconds = 10;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--list") {
            listOnly = true;
        } else if (argument == "--timeout" && i + 1 < argc) {
            timeoutSeconds = std::atoi(argv[++i]);
        } else if (argument == "--help" || argument == "-h") {
            std::printf("usage: %s [--list] [--timeout seconds] "
                        "[name-filter ...]\n",
                        argv[0]);
            return 0;
        } else {
            filters.push_back(argument);
        }
    }

    if (listOnly) {
        for (const test::TestCase &testCase : test::allTests()) {
            std::printf("%s\n", testCase.name.c_str());
        }
        return 0;
    }

    std::size_t passed = 0;
    std::size_t failed = 0;
    std::size_t skipped = 0;

    for (const test::TestCase &testCase : test::allTests()) {
        if (!filters.empty()) {
            bool matches = false;
            for (const std::string &filter : filters) {
                if (testCase.name.find(filter) != std::string::npos) {
                    matches = true;
                    break;
                }
            }
            if (!matches) {
                ++skipped;
                continue;
            }
        }

#ifdef TEST_RUN_ISOLATED
        // Each test runs in its own process so that a hang or a crash is
        // reported as a failure instead of stopping the whole run.
        const pid_t pid = fork();
        if (pid == 0) {
            test::Context context;
            testCase.fn(context);
            report(testCase, context);
            _exit(context.failures().empty() ? 0 : 1);
        }
        if (pid < 0) {
            std::printf("FAIL  %s\n        fork() failed\n",
                        testCase.name.c_str());
            ++failed;
            continue;
        }

        const int pollMicros = 1000;
        const int maxPolls = timeoutSeconds * 1000000 / pollMicros;
        int status = 0;
        bool finished = false;
        for (int poll = 0; poll < maxPolls; ++poll) {
            const pid_t reaped = waitpid(pid, &status, WNOHANG);
            if (reaped == pid) {
                finished = true;
                break;
            }
            if (reaped < 0) {
                break;
            }
            usleep(pollMicros);
        }

        if (!finished) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            std::printf("FAIL  %s\n        timed out after %d second(s)\n",
                        testCase.name.c_str(), timeoutSeconds);
            ++failed;
        } else if (WIFSIGNALED(status)) {
            std::printf("FAIL  %s\n        crashed with signal %d\n",
                        testCase.name.c_str(), WTERMSIG(status));
            ++failed;
        } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            ++passed;
        } else {
            ++failed;
        }
#else
        test::Context context;
        testCase.fn(context);
        report(testCase, context);
        if (context.failures().empty()) {
            ++passed;
        } else {
            ++failed;
        }
#endif
    }

    std::printf("\n%zu passed, %zu failed", passed, failed);
    if (skipped != 0) {
        std::printf(", %zu filtered out", skipped);
    }
    std::printf("\n");

    return failed == 0 ? 0 : 1;
}
