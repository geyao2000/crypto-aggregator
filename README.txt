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

1. Build the base image (pre-compiled heavy dependencies — only needed once or when deps change)

    sudo docker build -f dockerfile.base -t aggregator-base:latest .

2. Build aggregator server image

	sudo docker build -f docker/Dockerfile.aggregator -t aggregator-service:v1.0.6 .

3. Build the three clients imgages

	sudo docker build -f docker/Dockerfile.client-bbo -t client-bbo:v1.0.6 .
	sudo docker build -f docker/Dockerfile.client-volumebands -t client-volumebands:v1.0.6 .
	sudo docker build -f docker/Dockerfile.client-pricebands -t client-pricebands:v1.0.6 .

4. Create Network

	sudo docker network create my-trading-net
	
5. (Recommended) Use docker compose to build everything and start network at once 

	sudo docker compose up -d --build
	

## Run the System

	Option 1: (Recommended) Using docker compose 
	
	sudo docker compose up -d
	
	# Starts aggregator server + all three clients.
	# Clients automatically connect to aggregator:50051.
	# View status:
	
	sudo docker compose ps
	
	Option 2: Manual runs
	
	# Start aggregator server
	
	sudo docker run -d --name aggregator --network my-trading-net -p 50051:50051 aggregator-service:v1.0.5
	
	# Start clients (connect to aggregator)
	
	sudo docker run -d --name client-bbo --network my-trading-net client-bbo:v1.0.6
	sudo docker run -d --name client-volumebands --network my-trading-net client-volumebands:v1.0.6
	sudo docker run -d --name client-pricebands --network my-trading-net client-pricebands:v1.0.6
	
Testing

	1. Check server is running
	
	sudo docker logs -f aggregator
	# Look for "Aggregator gRPC server running on port 50051"

	2. Test subscription (requires grpcurl)
	
	grpcurl -plaintext -d '{}' localhost:50051 aggregator.AggregatorService/SubscribeBook
	#You should see real-time BookUpdate messages (timestamp_ms + bids/asks).

	3. Check client logs
	
	sudo docker logs -f client-bbo

Technical Decisions

	Multi-stage builds — Heavy compilation in builder stage, runtime image is minimal (~200MB).
	
	Pre-built base image (aggregator-base) — Contains compiled gRPC, Protobuf, Abseil, Boost etc. → fast incremental builds.
	
	Independent containers per service — Fault isolation, independent scaling/restart, clear logs/monitoring.
	
	docker-compose — Single command to start everything, automatic dependency ordering (depends_on).
	
	Proto files generated at build time — Keeps source tree clean (generated in build/generated).
	
	Static linking preference for heavy deps — Reduces runtime dependencies (though dynamic linking used here for compatibility).
	
	Manual json.hpp download — Avoids FetchContent network issues in Docker.

Dependencies

	gRPC v1.62.0
	
	Protobuf v3.25.3
	
	Abseil LTS 20230802.1
	
	Boost 1.74+
	
	nlohmann/json v3.11.3
	
	Ubuntu 22.04 base
	
	
