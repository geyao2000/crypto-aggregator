#include "binance_connector.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <iomanip>

#include "aggregator.h"

using json = nlohmann::json;
using namespace std;

BinanceConnector::BinanceConnector(Aggregator* aggregator)
    : Connector(aggregator, "Binance",0.1) {}

void BinanceConnector::parse_message(const std::string& msg) {
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
        // std::cout << "[Binance Debug] Raw message: " << msg << std::endl;  // 可选：调试时打开

        json j = json::parse(msg);

        // 订阅成功响应（{"result":null,"id":1} 或类似）
        if (j.contains("id") && (j.contains("result") && j["result"].is_null())) {
            std::cout << "[Binance] Subscription SUCCESS" << std::endl;
            return;
        }

        // 深度快照消息（每次都是完整 top N 档）
        if (j.contains("bids") && j.contains("asks") && j.contains("lastUpdateId")) {
            {
                std::lock_guard<std::mutex> lock(book_mutex_);
                local_bids_.clear();
                local_asks_.clear();
                // std::cout<<"test4"<<std::endl;
                for (const auto& level : j["bids"]) {
                    double raw_price = std::stod(level[0].get<std::string>());
                    double price = standardize_price(raw_price, tick_size_,true);
                    double qty = std::stod(level[1].get<std::string>());
                    // printf("raw: %10.2f, bid: %10.2f,%.8f\n",raw_price,price,qty);
                    // printf("bid: %10.1f,%.8f\n",price,qty);
                    if (qty > 0.0) local_bids_[price] += qty;
                }
                for (const auto& level : j["asks"]) {
                    double raw_price = std::stod(level[0].get<std::string>());
                    double price = standardize_price(raw_price, tick_size_,false);
                    double qty = std::stod(level[1].get<std::string>());
                    // printf("raw: %10.2f, ask: %10.2f,%.8f\n",raw_price,price,qty);
                    if (qty > 0.0) local_asks_[price] += qty;
                }
            }
            if (aggregator_) {
                aggregator_->on_book_updated(this);
            }
            // std::cout<<"test5"<<std::endl;
            // print_book();  // <--- 实时打印 Binance 更新
        }
    } catch (const std::exception& e) {
        std::cerr << "[Binance] Parse error: " << e.what() << std::endl;
    }
}
