#pragma once
#include "order.hpp"
#include <cstdint>
#include <list>
#include <map>
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
        void cancel(uint32_t OrderId);
        void modify();
        Order best_bid();
        Order best_ask();


};