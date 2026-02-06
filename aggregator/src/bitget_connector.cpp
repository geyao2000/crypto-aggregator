#include "bitget_connector.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <iomanip>

#include "aggregator.h"

using json = nlohmann::json;
using namespace std;

BitgetConnector::BitgetConnector(Aggregator* aggregator)
    : Connector(aggregator, "Bitget",0.1) {}

void BitgetConnector::parse_message(const std::string& msg) {
    // 过滤常见 pong 响应（纯文本或 JSON），避免解析错误
    std::string trimmed = msg;
    // 去除空格/换行
    trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());

    if (trimmed == "pong" || 
        trimmed == "{\"pong\":true}" || 
        trimmed == "{\"event\":\"pong\"}" ||  // 某些交易所格式
        trimmed.find("pong") != std::string::npos) {
        // 可选：std::cout << "[" << name_ << "] Received pong" << std::endl;
        return;
    }
    
    try {
        json j = json::parse(msg);

        // 订阅响应
        if (j.contains("code") && j["code"] == "0") {
           std::cout << "[Bitget] Subscription SUCCESS (books50)" << std::endl;
            return;
        }

        // books50快照
        if (j.contains("action") && j["action"] == "snapshot" &&
            j.contains("data") && j["data"].is_array() && !j["data"].empty()) {
            const auto& data = j["data"][0];

            if (data.contains("bids") && data.contains("asks")) {
                {
                    std::lock_guard<std::mutex> lock(book_mutex_);
                    local_bids_.clear();
                    local_asks_.clear();

                    for (const auto& level : data["bids"]) {
                        double raw_price = std::stod(level[0].get<std::string>());
                        double price = standardize_price(raw_price, tick_size_,true);
                        double qty = std::stod(level[1].get<std::string>());
                        // printf("raw: %10.2f, bid: %10.2f,%.8f\n",raw_price,price,qty);
                        if (qty > 0.0) local_bids_[price] += qty;
                    }
                    for (const auto& level : data["asks"]) {
                        double raw_price = std::stod(level[0].get<std::string>());
                        double price = standardize_price(raw_price, tick_size_,false);
                        double qty = std::stod(level[1].get<std::string>());
                        // printf("raw: %10.2f, ask: %10.2f,%.8f\n",raw_price,price,qty);
                        if (qty > 0.0) local_asks_[price] += qty;
                    }
                }
                // lock结束

                if (aggregator_) {
                    aggregator_->on_book_updated(this);
                }
                // std::cout<<"[Bitget]";
                //  print_book();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Bitget] Parse error: " << e.what() << std::endl;
    }
   
}