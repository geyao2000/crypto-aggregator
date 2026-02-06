# Crypto Orderbook Aggregator

Real-time aggregation of BTCUSDT orderbook data from multiple exchanges (Binance, OKX, Bitget, Bybit) with a unified gRPC streaming interface.

## Overview

This project aggregates depth updates from four major crypto exchanges and provides a single gRPC service for clients to subscribe to the merged orderbook (bids/asks). It supports:

- Real-time incremental + snapshot updates
- Standardized price/quantity precision
- Multi-client support (BBO, volume bands, price bands)

## Architecture

- **aggregator**: gRPC server that subscribes to WebSocket feeds from exchanges, merges orderbooks, and streams updates on port 50051.
- **client-bbo**: Best Bid/Offer client — subscribes and prints top bid/ask.
- **client-volume-bands**: Volume bands client — monitors volume in price ranges.
- **client-price-bands**: Price bands client — monitors price movements in ranges.

Each component runs in its own Docker container. The system uses docker-compose for orchestration on a single host.

## Tech Stack

- C++17
- gRPC v1.62.0 + Protobuf v3.25.3
- Abseil LTS 20230802.1
- Boost 1.74+ (system, thread)
- nlohmann/json v3.11.3 (header-only)
- WebSocket: Boost.Beast
- Base OS: Ubuntu 22.04

## Prerequisites

- Docker & Docker Compose installed
- Git

## Build Instructions

1. **Build the base image** (pre-compiled heavy dependencies — only needed once or when deps change)

   ```bash
   docker build -f dockerfile.base -t aggregator-base:latest .
