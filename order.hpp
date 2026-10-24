#pragma once
#include <cstdint>

enum class Side : uint8_t {
    Buy,
    Sell
};

struct Order {
    uint64_t id;
    uint32_t price;
    uint32_t qty;
    Side side;
};

