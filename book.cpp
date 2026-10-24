
#include "book.hpp"
#include "order.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <vector>


std::vector<Trade> Book::add(Order order){
    uint32_t &orderQty = order.qty;
    std::vector<Trade> trades;
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
            asks[order.price].push_back(order);
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
            bids[order.price].push_back(order);
        }

        return trades;   


    }

    return std::vector<Trade>{};
}

    
    
