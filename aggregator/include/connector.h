#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
// #include <nlohmann/json.hpp>
#include <map>
#include <mutex>
#include <string>
#include <functional>
#include <chrono>
#include <thread>
#include <cmath>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;
// using json = nlohmann::json;

class Aggregator;

class Connector {
public:
    // Connector();
    Connector(Aggregator* aggregator, const std::string& name, double tick_size);
    virtual ~Connector();

    void start();
    void print_book() const;
    std::map<double, double, std::greater<double>> get_bids_snapshot() const {
        std::lock_guard<std::mutex> lock(book_mutex_);
        return local_bids_;  // copy 返回
    }
    std::map<double, double> get_asks_snapshot() const {
        std::lock_guard<std::mutex> lock(book_mutex_);
        return local_asks_;
    }
    inline double standardize_price(double raw_price, double tick_size,bool is_bid) const {
        if (tick_size <= 0.0) return raw_price;  // 防除零
        if(is_bid){
            return int(raw_price/tick_size)*tick_size;
        } else {
            if(raw_price > int(raw_price / tick_size) * tick_size){
                return (int(raw_price/tick_size) + 1) * tick_size;
            }
            else{
                return raw_price;
            }
        }
    }
    // connector.h (class Connector protected 或全局)
    
    Aggregator* aggregator_{nullptr};  // 新增
    std::string name_;  // ← public
    std::map<double, double, std::greater<double>> local_bids_;  // public
    std::map<double, double> local_asks_;  // public
    mutable std::mutex book_mutex_;
protected:

    virtual bool needs_ping() const { return true; }  // <--- 默认需要 ping，其他交易所用 true
    double tick_size_{0.1};
    virtual std::string host() const = 0;
    virtual std::string port() const = 0;
    virtual std::string path() const = 0;
    virtual std::string subscribe_message() const = 0;
    virtual void parse_message(const std::string& msg) = 0;
    // Aggregator* aggregator_{nullptr};  // 新增

    // inline long long price_to_int(double raw_price) const {
    //     if (tick_size_ <= 0.0) return static_cast<long long>(std::round(raw_price * 100000000.0));  // fallback 高精度
    //     return static_cast<long long>(std::round(raw_price / tick_size_));
    // }

    // inline double int_to_price(long long price_int) const {
    //     if (tick_size_ <= 0.0) return price_int / 100000000.0;
    //     return price_int * tick_size_;
    // }
private:
    void run();
    void ping_loop();

    net::io_context ioc_;
    ssl::context ctx_{ssl::context::tlsv12_client};
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> ws_;
    std::thread thread_;
    std::thread ping_thread_;
    bool running_ = true;
};