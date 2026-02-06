// bybit_connector.cpp
#include "bybit_connector.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <iomanip>

#include "aggregator.h"  // 确保 aggregator_ 是完整类型

using json = nlohmann::json;
using namespace std;

BybitConnector::BybitConnector(Aggregator* aggregator)
    : Connector(aggregator, "Bybit",0.1) {}

void BybitConnector::parse_message(const std::string& msg) {
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
        // 可选调试打印（高频时建议注释，避免阻塞）
        // std::cout << "[Bybit] raw message received: " << msg << std::endl;

        json j = json::parse(msg);

        // 订阅成功响应
        if (j.contains("success") && j["success"] == true) {
            std::cout << "[Bybit] Subscription SUCCESS (orderbook.50.BTCUSDT)" << std::endl;
            return;
        }

        // 只处理 topic 为 orderbook.50.BTCUSDT 的消息
        if (!j.contains("topic") || j["topic"] != "orderbook.50.BTCUSDT") {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(book_mutex_);

            const auto& data = j["data"];

            // snapshot：完整快照，清空并重建本地订单簿
            if (j["type"] == "snapshot") {
                local_bids_.clear();
                local_asks_.clear();

                // bids
                for (const auto& level : data["b"]) {
                    double raw_price = std::stod(level[0].get<std::string>());
                    double price = standardize_price(raw_price, tick_size_,true);
                    double qty   = std::stod(level[1].get<std::string>());
                    if (qty > 0.0) {
                        local_bids_[price] = qty;
                    }
                }
                // asks
                for (const auto& level : data["a"]) {
                    double raw_price = std::stod(level[0].get<std::string>());
                    double price = standardize_price(raw_price, tick_size_,false);
                    double qty   = std::stod(level[1].get<std::string>());
                    if (qty > 0.0) {
                        local_asks_[price] = qty;
                    }
                }
            }
            // delta：增量更新（支持删除 qty="0"、更新、添加）
            else if (j["type"] == "delta") {
                // delete / update bids
                for (const auto& level : data["b"]) {
                    double price = std::stod(level[0].get<std::string>());
                    double qty   = std::stod(level[1].get<std::string>());
                    // printf("raw: %10.2f, bid: %10.2f,%.8f\n",raw_price,price,qty);
                    // printf("bid: %10.2f,%.8f\n",price,qty);
                    if (qty == 0.0) {
                        local_bids_.erase(price);
                    } else {
                        local_bids_[price] = qty;
                    }
                }
                // delete / update asks
                for (const auto& level : data["a"]) {
                    double price = std::stod(level[0].get<std::string>());
                    double qty   = std::stod(level[1].get<std::string>());
                    // printf("ask: %10.2f,%.8f\n",price,qty);
                    if (qty == 0.0) {
                        local_asks_.erase(price);
                    } else {
                        local_asks_[price] = qty;
                    }
                }
            }
        }
        // 每次有效更新后通知聚合器（可加 print_book() 如果需要打印）
        if (aggregator_) {
            aggregator_->on_book_updated(this);
        }
        // print_book();  // <--- 如需实时打印 Bybit 订单簿，取消注释

    } catch (const std::exception& e) {
        std::cerr << "[Bybit] Parse error: " << e.what() << std::endl;
    }
}