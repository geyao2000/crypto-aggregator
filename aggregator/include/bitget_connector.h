#pragma once

#include "connector.h"

class BitgetConnector : public Connector {
public:
    explicit BitgetConnector(Aggregator* aggregator);
protected:
    std::string host() const override { return "ws.bitget.com"; }
    std::string port() const override { return "443"; }
    std::string path() const override { return "/v2/ws/public"; }  // V2 公共端點
    std::string subscribe_message() const override {
        // return R"({"op":"subscribe","args":[{"instType":"SPOT","channel":"ticker","instId":"BTCUSDT"}]})";  // 永續合約 50 檔
        return R"({"op":"subscribe","args":[{"instType":"SPOT","channel":"books50","instId":"BTCUSDT"}]})";  // <--- 改成 books50
        // 如果想 15 檔： "channel":"books15"
        // 如果想增量更新： "channel":"books"
        //public md only level one
    }

    void parse_message(const std::string& msg) override;
};