"""Minimal demo of the order book extension, without Streamlit.

Build the extension first, then run this script:

    make bindings
    python3 python/demo.py
"""

from __future__ import annotations

from lobsim import MarketSession


def main() -> None:
    session = MarketSession()
    session.seed()

    print("-- seeded book --")
    print(
        "best bid:", session.best_bid,
        "| best ask:", session.best_ask,
        "| spread:", session.spread,
    )
    print("bids:", [(row["price"], row["quantity"]) for row in session.level_table(True)])

    print("\n-- aggressive sell sweeps the best bids --")
    result = session.submit(False, 98, 25)
    for trade in result.trades:
        print(f"  {trade['qty']} @ {trade['price']} (maker #{trade['maker']})")
    print("rested:", result.rested, "| remainder:", result.remainder)
    print("bids:", [(row["price"], row["quantity"]) for row in session.level_table(True)])
    print("asks:", [(row["price"], row["quantity"]) for row in session.level_table(False)])

    print("\n-- modify then cancel --")
    order_id = session.open_orders()[0].order_id
    print("modify:", session.modify(order_id, 96, 5))
    print("cancel:", session.cancel(order_id))
    print("open orders:", [order.order_id for order in session.open_orders()])

    print("\n-- depth --")
    for row in session.depth(3):
        print(row)


if __name__ == "__main__":
    main()
