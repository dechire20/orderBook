// pybind11 extension exposing BookFast to Python.
//
//   make bindings            # writes build/bookfast.<ext-suffix>
//   cmake -B build && cmake --build build   # builds it when pybind11 is found
//
// The module is a thin wrapper: it forwards to BookFast and converts the
// resting book to plain Python objects.

#include "BookFast.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

py::dict levelSnapshot(const std::map<uint32_t, std::list<Order>> &levels) {
    py::dict snapshot;
    for (const auto &[price, orders] : levels) {
        py::list ids;
        for (const Order &order : orders) {
            ids.append(order.id);
        }
        snapshot[py::int_(price)] = ids;
    }
    return snapshot;
}

} // namespace

PYBIND11_MODULE(bookfast, m) {
    m.doc() = "Price-time priority limit order book (BookFast engine)";

    py::enum_<Side>(m, "Side")
        .value("Buy", Side::Buy)
        .value("Sell", Side::Sell);

    py::class_<Order>(m, "Order")
        .def(py::init<uint64_t, uint32_t, uint32_t, Side>(), py::arg("id"),
             py::arg("price"), py::arg("qty"), py::arg("side"))
        .def_readwrite("id", &Order::id)
        .def_readwrite("price", &Order::price)
        .def_readwrite("qty", &Order::qty)
        .def_readwrite("side", &Order::side)
        .def("__repr__", [](const Order &order) {
            return "Order(id=" + std::to_string(order.id) +
                   ", price=" + std::to_string(order.price) +
                   ", qty=" + std::to_string(order.qty) +
                   ", side=" +
                   (order.side == Side::Buy ? "Side.Buy" : "Side.Sell") + ")";
        });

    py::class_<Trade>(m, "Trade")
        .def_readonly("maker_id", &Trade::maker_id)
        .def_readonly("taker_id", &Trade::taker_id)
        .def_readonly("price", &Trade::price)
        .def_readonly("qty", &Trade::qty)
        .def("__repr__", [](const Trade &trade) {
            return "Trade(maker_id=" + std::to_string(trade.maker_id) +
                   ", taker_id=" + std::to_string(trade.taker_id) +
                   ", price=" + std::to_string(trade.price) +
                   ", qty=" + std::to_string(trade.qty) + ")";
        });

    py::class_<BookFast>(m, "BookFast")
        .def(py::init<>())
        .def(
            "add",
            [](BookFast &book, uint64_t id, uint32_t price, uint32_t qty,
               Side side) { return book.add(Order{id, price, qty, side}); },
            py::arg("id"), py::arg("price"), py::arg("qty"), py::arg("side"),
            "Add an order; returns the trades it generated.")
        .def("cancel", &BookFast::cancel, py::arg("order_id"),
             "Cancel a resting order by id.")
        .def("modify", &BookFast::modify, py::arg("order_id"),
             py::arg("new_price"), py::arg("new_qty"),
             "Reprice/resize a resting order; returns any trades it caused.")
        .def("best_bid", &BookFast::best_bid)
        .def("best_ask", &BookFast::best_ask)
        .def("contains_order_id", &BookFast::containsOrderId,
             py::arg("order_id"))
        .def_property_readonly(
            "bid_levels",
            [](const BookFast &book) { return levelSnapshot(book.bidLevels()); },
            "Price -> resting order ids, best bid first.")
        .def_property_readonly(
            "ask_levels",
            [](const BookFast &book) { return levelSnapshot(book.askLevels()); },
            "Price -> resting order ids, best ask first.");
}
