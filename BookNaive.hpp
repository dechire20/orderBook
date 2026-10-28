#pragma once
#include "order.hpp"
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <vector>
#include <algorithm>


struct Trade {
    uint64_t maker_id;
    uint64_t taker_id;
    uint32_t price;
    uint32_t qty;
};


class Book {
    public:

        std::map<uint32_t, std::list<Order>> bids;
        std::map<uint32_t, std::list<Order>> asks;


        std::vector<Trade> add(Order);
        bool cancel(Side, uint64_t);
        std::vector<Trade> modify(Side, uint64_t, uint32_t, uint32_t);
        std::optional<uint32_t> best_bid();
        std::optional<uint32_t> best_ask();

        bool containsOrderId(uint64_t);


};