// Main umbrella header for the order-book library.
// Include this single header to access all public types and classes:
//
//   orderbook::OrderBook              — standard limit order book (tree-based)
//   orderbook::PriceCollarOrderBook   — price-collar variant (flat-array O(1) levels)
//   orderbook::ConcurrentOrderBook    — multi-producer serialised wrapper
//   orderbook::MarketMaker            — automated quoting strategy
//   orderbook::ArbitrageDetector      — cross-book arbitrage scanner
//   orderbook::Analytics              — VWAP, volatility, volume-profile analytics
//   orderbook::Order / Trade / etc.   — shared types (types.hpp)

#pragma once

#include "orderbook/types.hpp"
#include "orderbook/memory_pool.hpp"
#include "orderbook/price_level.hpp"
#include "orderbook/order_book.hpp"
#include "orderbook/order_book_impl.hpp"
#include "orderbook/concurrent_order_book.hpp"
#include "orderbook/market_maker.hpp"
#include "orderbook/arbitrage.hpp"
#include "orderbook/analytics.hpp"
#include "orderbook/price_collar_order_book.hpp"
