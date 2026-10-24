
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
    if (order.qty == 0){
        return std::vector<Trade>{};
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


void Book::cancel(Side side, uint64_t orderId){

    if (Side::Sell == side){

        for (auto it = asks.begin(); it != asks.end(); ++it){

            auto orderIt = it->second.begin();
            while (orderIt != it->second.end()){
                if (orderIt->id == orderId){
                    it->second.erase(orderIt);
                    if (it->second.empty()){
                        it = asks.erase(it);
                        return;
                    }
                    break;
                }
                else{
                    ++orderIt;
                }
            }


        }
    }
    else if(Side::Buy == side){

        for (auto it = bids.rbegin(); it != bids.rend(); ++it){

            auto orderIt = it->second.begin();
            while (orderIt != it->second.end()){
                if (orderIt->id == orderId){
                    it->second.erase(orderIt);
                    if (it->second.empty()){
                        auto eraseit = std::next(it).base();
                        bids.erase(eraseit);
                        return;
                    }
                    break;
                }
                else{
                    ++orderIt;
                }
            }


        }
    }
}

    
    
