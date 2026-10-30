#include "BookFast.hpp"
#include "order.hpp"
#include <cstdint>


std::vector<Trade> BookFast::add(Order order){

    uint32_t &orderQty = order.qty;
    std::vector<Trade> trades;

    if (order.qty == 0){
        return {};
    }

    if (orderIndex.find(order.id) != orderIndex.end()){
        return {};
    }


    if (order.side == Side::Sell){


        auto it = bids.rbegin();

        while(it != bids.rend()){

            if (it->first < order.price){
                break;
            }

            while(orderQty > 0 &&  !it->second.empty()){
                auto& restingOrder = it->second.front();

                uint32_t tradeQty = std::min(orderQty, restingOrder.qty);
                
                trades.push_back(Trade{
                    restingOrder.id,
                    order.id,
                    restingOrder.price,
                    tradeQty
                });

                orderQty -= tradeQty;
                restingOrder.qty -= tradeQty;

                if (restingOrder.qty == 0){
                    orderIndex.erase(restingOrder.id);
                    it->second.pop_front();
                }
                else{
                    break;
                }
            }

            if (it->second.empty()){
                auto eraseIt = std::next(it).base();
                it = std::make_reverse_iterator(bids.erase(eraseIt));
            }
            else {
                ++it;
            }
            
            if (orderQty == 0){
                break;
            }
        }
        if (orderQty > 0){

            auto &orders = asks[order.price];
            orders.push_back(order);

            orderIndex.emplace(order.id,
                               OrderLocation{order.side, order.price,
                                             std::prev(orders.end())});
        }

        return trades;      
    }           
        
    else if (order.side == Side::Buy){

        auto it = asks.begin();

        while (it != asks.end()){

            if (it->first > order.price){
                break;
            }
            while(orderQty > 0 &&  !it->second.empty()){
                auto& restingOrder = it->second.front();

                uint32_t tradeQty = std::min(orderQty, restingOrder.qty);
                
                trades.push_back(Trade{
                    restingOrder.id,
                    order.id,
                    restingOrder.price,
                    tradeQty
                });

                orderQty -= tradeQty;
                restingOrder.qty -= tradeQty;

                if (restingOrder.qty == 0){
                    orderIndex.erase(restingOrder.id);
                    it->second.pop_front();
                }
                else{
                    break;
                }
            }

            if (it->second.empty()){
                it = asks.erase(it);
            }
            else {
                ++it;
            }
            
            if (orderQty == 0){
                break;
            }
        }
        if (orderQty > 0){

            auto &orders = bids[order.price];
            orders.push_back(order);
            orderIndex.emplace(order.id,
                               OrderLocation{order.side, order.price,
                                             std::prev(orders.end())});

        }

        return trades;   


    }

    return std::vector<Trade>{};
}


bool BookFast::cancel(uint64_t orderId){

    auto indexIt = orderIndex.find(orderId);

    if (indexIt == orderIndex.end()){
        return false;
    }

    auto level = indexIt->second.price;


    if (Side::Sell == indexIt->second.side){


        auto levelIt = asks.find(level);

        if (levelIt == asks.end()){
            return false;
        }

        levelIt->second.erase((indexIt->second.it));

        if (levelIt->second.empty()){
            asks.erase(levelIt);
        }

        orderIndex.erase(indexIt);

        return true;
    }
    else if(Side::Buy == indexIt->second.side){


        auto levelIt = bids.find(level);

        if (levelIt == bids.end()){
            return false;
        }

        levelIt->second.erase((indexIt->second.it));

        if (levelIt->second.empty()){
            bids.erase(levelIt);
        }

        orderIndex.erase(indexIt);

        return true;
    }    

    return false;
}


std::vector<Trade> BookFast::modify(uint64_t orderId, uint32_t newPrice, uint32_t newQty){

    auto indexIt = orderIndex.find(orderId);

    if (indexIt == orderIndex.end()){
        return {};
    }

    Side side = indexIt->second.side;

    if (newQty != 0 && cancel(orderId)){
        return add(Order{orderId, newPrice, newQty, side});
    }
    else{
        return std::vector<Trade>{};
    }
}

bool BookFast::containsOrderId(uint64_t id) const {
    return orderIndex.find(id) != orderIndex.end();
}

std::optional<uint32_t> BookFast::best_bid() {
    if (bids.empty()){
        return std::nullopt;
    }
    return bids.rbegin()->first;
}

std::optional<uint32_t> BookFast::best_ask() {
    if (asks.empty()){
        return std::nullopt;
    }
    return asks.begin()->first;
}
