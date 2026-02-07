#pragma once

#include "connector.h"

class BinanceConnector : public Connector {
public:
    explicit BinanceConnector(Aggregator* aggregator);  
protected:
    std::string host() const override { return "stream.binance.com"; }
    std::string port() const override { return "9443"; }
    std::string path() const override { return "/ws"; }
    std::string subscribe_message() const override {
        return R"({"method":"SUBSCRIBE","params":["btcusdt@depth20@100ms"],"id":1})";
        // return R"({"method":"SUBSCRIBE","params":["btcusdt@depth5@100ms"],"id":1})";
        // return R"({"method":"SUBSCRIBE","params":["btcusdt@depth20@100ms"],"id":1})";
        // 如果想 5 檔測試：R"({"method":"SUBSCRIBE","params":["btcusdt@depth5@100ms"],"id":1})"
    }

    void parse_message(const std::string& msg) override;
    bool needs_ping() const override { return false; }  // <--- Binance 不需要 ping
};