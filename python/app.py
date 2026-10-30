"""Streamlit front-end for the BookFast limit order book engine.

Run it with:

    make bindings
    python3 -m streamlit run python/app.py
"""

from __future__ import annotations

import altair as alt
import pandas as pd
import streamlit as st

from lobsim import MarketSession

BID_COLOR = "#26a69a"
ASK_COLOR = "#ef5350"
LEVEL_HEIGHT = 35
MAX_LEVELS_SHOWN = 12
TRADE_TAPE_ROWS = 20


# --------------------------------------------------------------------- helpers


def _session() -> MarketSession:
    if "session" not in st.session_state:
        st.session_state.session = MarketSession()
    return st.session_state.session


def _flash(message: str, kind: str = "success") -> None:
    st.session_state.setdefault("flash", []).append((kind, message))


def _render_flash() -> None:
    for kind, message in st.session_state.pop("flash", []):
        getattr(st, kind)(message)


def _fmt(value: int | None) -> str:
    return "—" if value is None else f"{value:,}"


def _default_price(session: MarketSession) -> int:
    if session.best_bid is not None and session.best_ask is not None:
        return (session.best_bid + session.best_ask) // 2
    if session.last_price is not None:
        return session.last_price
    return 100


# ---------------------------------------------------------------- interactions


def submit_order(session: MarketSession, is_buy: bool, price: int, qty: int) -> None:
    result = session.submit(is_buy, price, qty)
    side = "BUY" if is_buy else "SELL"

    if result.traded:
        fills = ", ".join(f"{trade['qty']} @ {trade['price']}" for trade in result.trades)
        message = f"{side} {qty} @ {price} matched {fills}"
        if result.rested:
            message += f" · {result.remainder} rested as #{result.order_id}"
    elif result.rested:
        message = f"{side} {qty} @ {price} rested as #{result.order_id}"
    else:
        message = f"{side} {qty} @ {price} was rejected"

    _flash(message)


# ------------------------------------------------------------------------ views


def render_order_entry(session: MarketSession) -> None:
    with st.sidebar:
        st.subheader("Order entry")
        with st.form("order_entry"):
            is_buy = (
                st.radio("Side", ["BUY", "SELL"], horizontal=True, key="entry_side")
                == "BUY"
            )
            price_col, qty_col = st.columns(2)
            price = price_col.number_input(
                "Price",
                min_value=1,
                value=_default_price(session),
                step=1,
                key="entry_price",
            )
            qty = qty_col.number_input(
                "Quantity", min_value=1, value=10, step=1, key="entry_qty"
            )
            submitted = st.form_submit_button("Submit order", width="stretch")

        if submitted:
            submit_order(session, is_buy, int(price), int(qty))
            st.rerun()

        st.divider()
        st.subheader("Session")
        if st.button("Load sample book", width="stretch"):
            session.seed()
            _flash("Sample book loaded")
            st.rerun()
        if st.button("Reset book", width="stretch"):
            session.reset()
            _flash("Book reset")
            st.rerun()


def render_metrics(session: MarketSession) -> None:
    best_bid, best_ask, spread, last = st.columns(4)
    best_bid.metric("Best bid", _fmt(session.best_bid))
    best_ask.metric("Best ask", _fmt(session.best_ask))
    spread.metric("Spread", _fmt(session.spread))
    last.metric("Last price", _fmt(session.last_price))
    st.caption(
        f"{len(session.open_orders()):,} resting orders · "
        f"{len(session.trades):,} trades · {session.volume:,} traded quantity"
    )


def _levels_table(rows: list[dict], peak: int) -> None:
    if not rows:
        st.caption("No resting orders")
        return
    frame = pd.DataFrame(rows, columns=["price", "quantity", "orders"])
    st.dataframe(
        frame,
        hide_index=True,
        width="stretch",
        height=LEVEL_HEIGHT * min(len(rows), MAX_LEVELS_SHOWN) + 38,
        column_config={
            "price": st.column_config.NumberColumn("Price", format="localized"),
            "quantity": st.column_config.ProgressColumn(
                "Quantity", format="%d", min_value=0, max_value=peak
            ),
            "orders": st.column_config.NumberColumn("Orders", width="small"),
        },
    )


def render_book(session: MarketSession) -> None:
    bids = session.level_table(True)[:MAX_LEVELS_SHOWN]
    asks = session.level_table(False)[:MAX_LEVELS_SHOWN]
    peak = max([row["quantity"] for row in bids + asks] + [1])

    st.subheader("Order book")
    bid_col, ask_col = st.columns(2)
    with bid_col:
        st.markdown(f":green[**BIDS**]")
        _levels_table(bids, peak)
    with ask_col:
        st.markdown(f":red[**ASKS**]")
        _levels_table(asks, peak)


def render_depth_chart(session: MarketSession) -> None:
    st.subheader("Market depth")
    rows = session.depth(MAX_LEVELS_SHOWN)
    if not rows:
        st.caption("No resting orders")
        return

    frame = pd.DataFrame(rows)
    chart = (
        alt.Chart(frame)
        .mark_area(interpolate="step-after", opacity=0.35, line=True)
        .encode(
            x=alt.X("price:Q", title="Price", axis=alt.Axis(format="d")),
            y=alt.Y("cumulative:Q", title="Cumulative quantity"),
            color=alt.Color(
                "side:N",
                scale=alt.Scale(domain=["bid", "ask"], range=[BID_COLOR, ASK_COLOR]),
                legend=None,
            ),
            tooltip=["side", "price", "quantity", "cumulative"],
        )
    )
    if session.best_bid is not None and session.best_ask is not None:
        mid = (session.best_bid + session.best_ask) / 2
        chart += (
            alt.Chart(pd.DataFrame({"mid": [mid]}))
            .mark_rule(strokeDash=[4, 4], color="#888")
            .encode(x="mid:Q")
        )
    st.altair_chart(chart, width="stretch")


def render_trades(session: MarketSession) -> None:
    st.subheader("Trade tape")
    if not session.trades:
        st.caption("No trades yet")
        return
    recent = list(reversed(session.trades))[:TRADE_TAPE_ROWS]
    frame = pd.DataFrame(recent)[["maker", "taker", "price", "qty"]]
    frame.columns = ["Maker", "Taker", "Price", "Quantity"]
    st.dataframe(frame, hide_index=True, width="stretch", height=LEVEL_HEIGHT * len(frame) + 38)


def render_open_orders(session: MarketSession) -> None:
    st.subheader("Open orders")
    orders = session.open_orders()
    if not orders:
        st.caption("No resting orders")
        return

    frame = pd.DataFrame(
        [
            {
                "id": order.order_id,
                "side": order.side_label,
                "price": order.price,
                "remaining": order.qty,
                "filled": order.filled,
                "status": order.status,
            }
            for order in orders
        ]
    )
    st.dataframe(
        frame,
        hide_index=True,
        width="stretch",
        height=LEVEL_HEIGHT * min(len(frame), MAX_LEVELS_SHOWN) + 38,
    )

    order_ids = [order.order_id for order in orders]
    cancel_tab, modify_tab = st.tabs(["Cancel", "Modify"])

    with cancel_tab:
        cancel_id = st.selectbox("Order id", order_ids, key="cancel_order")
        if st.button("Cancel order", width="stretch"):
            if session.cancel(int(cancel_id)):
                _flash(f"Cancelled #{cancel_id}")
            else:
                _flash(f"#{cancel_id} is no longer resting", "warning")
            st.rerun()

    with modify_tab:
        modify_id = st.selectbox("Order id", order_ids, key="modify_order")
        order = session.order(int(modify_id))
        price = st.number_input(
            "New price", min_value=1, value=int(order.price), step=1, key=f"price_{modify_id}"
        )
        qty = st.number_input(
            "New quantity", min_value=1, value=int(order.qty), step=1, key=f"qty_{modify_id}"
        )
        if st.button("Apply modification", width="stretch"):
            session.modify(int(modify_id), int(price), int(qty))
            _flash(f"Modified #{modify_id} to {price} × {qty}")
            st.rerun()


def main() -> None:
    st.set_page_config(page_title="Limit Order Book Simulator", page_icon="📈", layout="wide")

    session = _session()

    st.title("Limit Order Book Simulator")
    st.caption(
        "BookFast matching engine (C++17) driven through pybind11 — simulated liquidity, "
        "no real exchange, accounts or money."
    )

    render_order_entry(session)
    _render_flash()
    render_metrics(session)

    st.divider()
    book_col, depth_col = st.columns([3, 2])
    with book_col:
        render_book(session)
    with depth_col:
        render_depth_chart(session)

    st.divider()
    trades_col, orders_col = st.columns(2)
    with trades_col:
        render_trades(session)
    with orders_col:
        render_open_orders(session)


if __name__ == "__main__":
    main()
