#include "okx_connector.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <iomanip>

#include "aggregator.h"

using json = nlohmann::json;

OKXConnector::OKXConnector(Aggregator* aggregator)
    : Connector(aggregator, "OKX",0.1) {}

void OKXConnector::parse_message(const std::string& msg) {
    // <--- 新增：过滤 pong 响应（常见格式）
    std::string trimmed = msg;
    // 去除空白字符
    trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());

    if (trimmed == "pong" ||
        trimmed == "{\"pong\":true}" ||
        trimmed == "{\"event\":\"pong\"}" ||
        trimmed.rfind("pong", 0) == 0) {  // 以 "pong" 开头兜底
        // 可选日志：std::cout << "[" << name_ << "] Ignored pong response" << std::endl;
        return;  // 直接返回，不解析
    }
    try {
        json j = json::parse(msg);
        if (j.contains("event") && j["event"] == "subscribe") {
            std::cout << "[OKX] Subscription SUCCESS (books50)" << std::endl;
            return;
        }
        if (j.contains("data") && j["data"].is_array() && !j["data"].empty()) {
            const auto& data = j["data"][0];

            if (data.contains("bids") && data.contains("asks")) {
                {
                    std::lock_guard<std::mutex> lock(book_mutex_);
                    local_bids_.clear();
                    local_asks_.clear();

                    for (const auto& level : data["bids"]) {
                        // double raw_price = std::stod(level[0].get<std::string>());
                        // double price = standardize_price(raw_price, tick_size_,true);
                        double price = std::stod(level[0].get<std::string>());
                        double qty = std::stod(level[1].get<std::string>());
                        // printf("raw: %10.2f, bid: %10.2f,%.8f\n",raw_price,price,qty);
                        // printf("bid: %10.2f,%.8f\n",price,qty);
                        if (qty > 0.0) local_bids_[price] += qty;
                    }
                    for (const auto& level : data["asks"]) {
                        // double raw_price = std::stod(level[0].get<std::string>());
                        // double price = standardize_price(raw_price, tick_size_,false);
                        double price = std::stod(level[0].get<std::string>());
                        double qty = std::stod(level[1].get<std::string>());
                        // printf("raw: %10.2f, ask: %10.2f,%.8f\n",raw_price,price,qty);
                        // printf("ask: %10.2f,%.8f\n",price,qty);
                        if (qty > 0.0) local_asks_[price] += qty;
                    }
                    // lock结束
                }
                if (aggregator_) {
                    aggregator_->on_book_updated(this);
                }
                // std::cout<<"[OKX]";
                // print_book();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
    }
    
}