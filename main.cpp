#include <cstdint>
#include <functional>
#include <iostream>
#include <list>
#include <map>

class Order
{
    public:
        Order();
};


int main() {

    std::map<uint32_t, std::list<Order>, std::greater<>> bids;
    std::map<uint32_t, std::list<Order>> asks;




    return 0;
}
