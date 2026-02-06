// bybit_connector.h
#pragma once

#include "connector.h"

class BybitConnector : public Connector {
public:
    explicit BybitConnector(Aggregator* aggregator);

protected:
    std::string host() const override { return "stream.bybit.com"; }
    std::string port() const override { return "443"; }
    std::string path() const override { return "/v5/public/spot"; }
    std::string subscribe_message() const override {
        return R"({"op":"subscribe","args":["orderbook.50.BTCUSDT"]})";  // top 50 档（snapshot + incremental）
    }

    void parse_message(const std::string& msg) override;
};