#pragma once

#include "connector.h"
#include <map>
#include <string>

class OKXConnector : public Connector {
public:
    // OKXConnector(Aggregator* agg) : Connector(agg, "OKX") {}
    explicit OKXConnector(Aggregator* aggregator);
protected:
    std::string host() const override { return "ws.okx.com"; }
    std::string port() const override { return "8443"; }
    std::string path() const override { return "/ws/v5/public"; }
    std::string subscribe_message() const override {
        return R"({"op":"subscribe","args":[{"channel":"books5","instId":"BTC-USDT"}]})";
        // return R"({"op":"subscribe","args":[{"channel":"books50","instId":"BTC-USDT"}]})";
    }

    void parse_message(const std::string& msg) override;
};