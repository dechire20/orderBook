"""Minimal demo of the bookfast Python extension.

Build the extension first, then run this script:

    make bindings
    python3 python/bookfast_demo.py
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIRS = (REPO_ROOT / "build", REPO_ROOT / "build-cmake")


def load_bookfast():
    for build_dir in BUILD_DIRS:
        if any(build_dir.glob("bookfast*.so")):
            sys.path.insert(0, str(build_dir))
            break
    try:
        import bookfast
    except ImportError as exc:
        raise SystemExit(
            f"bookfast module not found ({exc}); run `make bindings` first."
        )
    return bookfast


def main() -> None:
    bookfast = load_bookfast()
    side = bookfast.Side
    book = bookfast.BookFast()

    print("-- empty book --")
    print("best bid:", book.best_bid(), "| best ask:", book.best_ask())

    print("\n-- resting asks --")
    book.add(1, 100, 10, side.Sell)
    book.add(2, 101, 10, side.Sell)
    print("ask levels:", book.ask_levels)
    print("best ask:", book.best_ask())

    print("\n-- aggressive buy hits the best ask --")
    for trade in book.add(3, 100, 4, side.Buy):
        print(f"  {trade}")
    print("ask levels:", book.ask_levels)

    print("\n-- resting bid, then repriced --")
    book.add(4, 98, 5, side.Buy)
    book.modify(4, 99, 5)
    print("bid levels:", book.bid_levels)
    print("best bid:", book.best_bid())

    print("\n-- cancel --")
    print("cancel(1):", book.cancel(1), "| ask levels:", book.ask_levels)
    print("contains 1:", book.contains_order_id(1))


if __name__ == "__main__":
    main()
