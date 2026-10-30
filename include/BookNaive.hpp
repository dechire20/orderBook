#pragma once
#include "Order.hpp"
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <vector>
#include <algorithm>


class BookNaive {
    public:
        std::vector<Trade> add(Order);
        bool cancel(Side, uint64_t);
        std::vector<Trade> modify(Side, uint64_t, uint32_t, uint32_t);
        std::optional<uint32_t> best_bid();
        std::optional<uint32_t> best_ask();
        bool containsOrderId(uint64_t);
    
        std::map<uint32_t, std::list<Order>> bids;
        std::map<uint32_t, std::list<Order>> asks;


};