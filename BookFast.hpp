#pragma once

#include "order.hpp"

#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>


class BookFast {
    public:
        std::vector<Trade> add(Order);
        bool cancel(uint64_t);
        std::vector<Trade> modify(uint64_t, uint32_t, uint32_t);
        std::optional<uint32_t> best_bid();
        std::optional<uint32_t> best_ask();
        bool containsOrderId(uint64_t) const;

        // Read-only views of the price levels, mainly for tests and benchmarks.
        const std::map<uint32_t, std::list<Order>> &bidLevels() const {
            return bids;
        }
        const std::map<uint32_t, std::list<Order>> &askLevels() const {
            return asks;
        }
    private:
        struct OrderLocation{
            Side side;
            uint32_t price;
            std::list<Order>::iterator it;
        };
    private:
        std::map<uint32_t, std::list<Order>> bids;
        std::map<uint32_t, std::list<Order>> asks;
        std::unordered_map<uint64_t, OrderLocation> orderIndex;


};
