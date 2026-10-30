"""Adapter around the compiled BookFast extension.

The matching engine is authoritative: every submit/cancel/modify call goes
straight to C++. The extension only publishes order *ids* per price level, so
this module keeps a light mirror of the resting orders (price, remaining
quantity, filled quantity) to drive the order book tables and the open orders
panel. The mirror is kept in sync from the ``Trade`` objects the engine returns.
"""

from __future__ import annotations

import importlib
import sys
from dataclasses import dataclass, field
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_BUILD_DIRS = (_REPO_ROOT / "build", _REPO_ROOT / "build-cmake")
_MODULE_NAMES = ("book", "bookfast")

UINT32_MAX = 2**32 - 1


def load_engine_module():
    """Import the extension, looking in the usual build directories."""
    for build_dir in _BUILD_DIRS:
        if build_dir.is_dir() and str(build_dir) not in sys.path:
            sys.path.append(str(build_dir))

    errors = []
    for name in _MODULE_NAMES:
        try:
            return importlib.import_module(name)
        except ImportError as exc:
            errors.append(f"{name}: {exc}")

    raise RuntimeError(
        "cannot import the order book extension ("
        + "; ".join(errors)
        + "). Build it first with `make bindings`."
    )


@dataclass
class OrderView:
    """A resting order as the UI sees it."""

    order_id: int
    is_buy: bool
    price: int
    qty: int
    filled: int = 0

    @property
    def side_label(self) -> str:
        return "BUY" if self.is_buy else "SELL"

    @property
    def original_qty(self) -> int:
        return self.filled + self.qty

    @property
    def status(self) -> str:
        return "partially filled" if self.filled else "open"


@dataclass
class SubmitResult:
    order_id: int
    traded: int
    remainder: int
    rested: bool
    trades: list[dict] = field(default_factory=list)


class MarketSession:
    """Owns one BookFast instance plus the display mirror around it."""

    def __init__(self, module=None):
        self._module = module or load_engine_module()
        self._orders: dict[int, OrderView] = {}
        self.trades: list[dict] = []
        self.volume = 0
        self.last_price: int | None = None
        self._next_id = 1
        self._book = self._module.BookFast()

    # ------------------------------------------------------------ engine calls

    def submit(self, is_buy: bool, price: int, qty: int) -> SubmitResult:
        _validate(price, qty)
        order_id = self._take_id()
        raw = self._book.add(order_id, price, qty, self._side(is_buy))
        self._record(raw, order_id)

        traded = sum(trade.qty for trade in raw if trade.taker_id == order_id)
        remainder = qty - traded
        rested = remainder > 0 and self._book.contains_order_id(order_id)
        if rested:
            self._orders[order_id] = OrderView(order_id, is_buy, price, remainder, traded)

        return SubmitResult(order_id, traded, remainder, rested, self.trades[: -len(raw)])

    def cancel(self, order_id: int) -> bool:
        if self._book.cancel(order_id):
            self._orders.pop(order_id, None)
            return True
        return False

    def modify(self, order_id: int, price: int, qty: int) -> bool:
        order = self._orders.get(order_id)
        if order is None:
            return False
        _validate(price, qty)

        raw = self._book.modify(order_id, price, qty)
        self._record(raw, order_id)

        traded = sum(trade.qty for trade in raw if trade.taker_id == order_id)
        remainder = qty - traded
        order.filled += traded
        if remainder > 0 and self._book.contains_order_id(order_id):
            order.price = price
            order.qty = remainder
        else:
            self._orders.pop(order_id, None)
        return True

    # ------------------------------------------------------------------ views

    @property
    def best_bid(self) -> int | None:
        return self._book.best_bid()

    @property
    def best_ask(self) -> int | None:
        return self._book.best_ask()

    @property
    def spread(self) -> int | None:
        if self.best_bid is None or self.best_ask is None:
            return None
        return self.best_ask - self.best_bid

    def level_table(self, is_buy: bool) -> list[dict]:
        """Aggregated resting quantity per price, best price first."""
        levels: dict[int, dict] = {}
        for order in self._orders.values():
            if order.is_buy != is_buy:
                continue
            row = levels.setdefault(
                order.price, {"price": order.price, "quantity": 0, "orders": 0}
            )
            row["quantity"] += order.qty
            row["orders"] += 1
        return sorted(levels.values(), key=lambda row: row["price"], reverse=is_buy)

    def depth(self, max_levels: int = 12) -> list[dict]:
        """Cumulative quantity walking outwards from the spread."""
        rows = []
        for side, is_buy in (("bid", True), ("ask", False)):
            cumulative = 0
            for row in self.level_table(is_buy)[:max_levels]:
                cumulative += row["quantity"]
                rows.append(
                    {
                        "side": side,
                        "price": row["price"],
                        "quantity": row["quantity"],
                        "cumulative": cumulative,
                    }
                )
        return rows

    def open_orders(self) -> list[OrderView]:
        return sorted(
            self._orders.values(),
            key=lambda order: (not order.is_buy, -order.price if order.is_buy else order.price),
        )

    def order(self, order_id: int) -> OrderView | None:
        return self._orders.get(order_id)

    # ---------------------------------------------------------------- helpers

    def reset(self) -> None:
        self._book = self._module.BookFast()
        self._orders.clear()
        self.trades.clear()
        self.volume = 0
        self.last_price = None
        self._next_id = 1

    def seed(self) -> None:
        """Prefill a small two-sided book so the UI is not empty."""
        self.reset()
        for offset, (bid_qty, ask_qty) in enumerate(
            [(15, 10), (20, 18), (12, 25), (8, 14), (30, 6)], start=1
        ):
            self.submit(True, 100 - offset, bid_qty)
            self.submit(False, 100 + offset, ask_qty)

    def _record(self, raw, taker_id: int) -> None:
        for trade in raw:
            maker = self._orders.get(trade.maker_id)
            if maker is not None:
                maker.filled += trade.qty
                maker.qty -= trade.qty
                if maker.qty <= 0:
                    self._orders.pop(trade.maker_id, None)
            self.trades.append(
                {
                    "maker": trade.maker_id,
                    "taker": trade.taker_id,
                    "price": trade.price,
                    "qty": trade.qty,
                }
            )
            self.volume += trade.qty
            self.last_price = trade.price

    def _side(self, is_buy: bool):
        return self._module.Side.Buy if is_buy else self._module.Side.Sell

    def _take_id(self) -> int:
        order_id = self._next_id
        self._next_id += 1
        return order_id


def _validate(price: int, qty: int) -> None:
    if not 0 < price <= UINT32_MAX:
        raise ValueError(f"price must be between 1 and {UINT32_MAX}, got {price}")
    if not 0 < qty <= UINT32_MAX:
        raise ValueError(f"quantity must be between 1 and {UINT32_MAX}, got {qty}")
