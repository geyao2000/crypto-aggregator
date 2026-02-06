#include "connector.h"
#include <iostream>
#include <chrono>
#include <iomanip>  // <--- 新增：提供 std::put_time, std::setfill, std::setw, std::fixed, std::setprecision 等
#include <ctime>    // <--- 新增：提供 std::localtime, std::tm 等

Connector::Connector(Aggregator* aggregator, const std::string& name, double tick_size)
    : aggregator_(aggregator), name_(name),tick_size_(tick_size)
{
    ctx_.set_verify_mode(ssl::verify_peer);
    ctx_.set_default_verify_paths();
}

// Connector::~Connector() {
//     running_ = false;
//     if (thread_.joinable()) thread_.join();
//     if (ping_thread_.joinable()) ping_thread_.join();
// }

Connector::~Connector() {
    std::cout << "[" << name_ << "] Destructor called, shutting down..." << std::endl;
    running_ = false;

    // 安全关闭 WebSocket
    if (ws_) {
        beast::error_code ec;
        ws_->close(websocket::close_code::going_away, ec);
        if (ec) {
            std::cerr << "[" << name_ << "] Destructor close error: " << ec.message() << std::endl;
        }
    }

    if (thread_.joinable()) {
        thread_.join();
        std::cout << "[" << name_ << "] Main thread joined" << std::endl;
    }
    if (ping_thread_.joinable()) {
        ping_thread_.join();
        std::cout << "[" << name_ << "] Ping thread joined" << std::endl;
    }
}

void Connector::start() {
    std::cout << "[" << name_ << "] Starting connector..." << std::endl;
    thread_ = std::thread(&Connector::run, this);
    // ping_thread_ = std::thread(&Connector::ping_loop, this);
    if (needs_ping()) {  // <--- 新增条件判断
        ping_thread_ = std::thread(&Connector::ping_loop, this);
    }
}

void Connector::print_book() const {
    std::lock_guard<std::mutex> lock(book_mutex_);

    if (local_bids_.empty() && local_asks_.empty()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::time_t timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};  // 需要 <ctime>

    localtime_r(&timer, &bt);  // <--- 用线程安全的 localtime_r 替换 std::localtime

    std::cout << "\n=== [" << name_ << "] BTCUSDT 订单簿更新 ===" 
              << std::put_time(&bt, "%Y-%m-%d %H:%M:%S")  // 需要 <iomanip>
              << '.' << std::setfill('0') << std::setw(3) << ms.count() << " ===\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Bids (买盘 Top 10):\n";
    int count = 0;
    for (auto it = local_bids_.begin(); it != local_bids_.end() && count < 10; ++it, ++count) {
        std::cout << "  Price: " << std::setw(12) << it->first
                  << "  Qty: " << std::setw(12) << it->second << "\n";
    }

    std::cout << "Asks (卖盘 Top 10):\n";
    count = 0;
    for (const auto& [price, qty] : local_asks_) {
        if (count++ >= 10) break;
        std::cout << "  Price: " << std::setw(12) << price
                  << "  Qty: " << std::setw(12) << qty << "\n";
    }
    std::cout << "==============================================\n\n";
}

void Connector::run() {
    while(running_){
        try {
            std::cout << "[" << name_ << "] Resolving host " << host() << ":" << port() << "..." << std::endl;
            
            tcp::resolver resolver(ioc_);
            auto const results = resolver.resolve(host(), port());

            if (results.empty()) {
                std::cerr << "[" << name_ << "] Resolve failed: no endpoints" << std::endl;
                return;
            }
            std::cout << "[" << name_ << "] Creating WebSocket stream..." << std::endl;

            ws_ = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(ioc_, ctx_);
            if (!ws_) {
                std::cerr << "[" << name_ << "] Failed to allocate WebSocket stream (nullptr)" << std::endl;
                return;
            }
            
            std::cout << "[" << name_ << "] Connecting TCP..." << std::endl;
            net::connect(ws_->next_layer().next_layer(), results.begin(), results.end());

            std::cout << "[" << name_ << "] Setting SNI..." << std::endl;
            if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host().c_str())) {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                std::cerr << "[" << name_ << "] SNI failed: " << ec.message() << std::endl;
                return;
            }
        
            std::cout << "[" << name_ << "] SSL handshake..." << std::endl;
            ws_->next_layer().handshake(ssl::stream_base::client);

            std::cout << "[" << name_ << "] WebSocket handshake..." << std::endl;
            ws_->handshake(host() + ":" + port(), path());

            // std::cout << "WebSocket connected to " << host() << std::endl;
            std::cout << "[" << name_ << "] WebSocket CONNECTED and subscribed" << std::endl;
            ws_->write(net::buffer(subscribe_message()));
            std::cout << "[" << name_ << "] Subscription sent: " << subscribe_message() << std::endl;
            // std::cout << "Subscription sent: " << subscribe_message() << std::endl;

            beast::flat_buffer buffer;
            
            while (running_) {
                beast::error_code ec;
                ws_->read(buffer, ec);

                if (ec) {
                    if (ec == websocket::error::closed ||
                        ec == boost::asio::error::eof ||
                        ec == boost::asio::error::operation_aborted) {
                        std::cout << "[" << name_ << "] WebSocket closed normally: " << ec.message() << std::endl;
                    } else {
                        std::cerr << "[" << name_ << "] WebSocket read error: " << ec.message() 
                                << " (code: " << ec.value() << ")" << std::endl;
                    }
                    break;
                }

                std::string msg = beast::buffers_to_string(buffer.data());
                buffer.consume(buffer.size());

                parse_message(msg);
            }
        // } catch (std::exception const& e) {
        //     std::cerr << "Error: " << e.what() << std::endl;
        } catch (const beast::error_code& ec) {
            std::cerr << "[" << name_ << "] Beast error: " << ec.message() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[" << name_ << "] Exception in run: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[" << name_ << "] Unknown exception in run" << std::endl;
        }

        // 重连前延迟（避免洪泛）
        if (running_) {
            std::cout << "[" << name_ << "] Reconnecting in 5 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        // 安全清理旧 ws_
        if (ws_) {
            beast::error_code ec;
            ws_->close(websocket::close_code::normal, ec);
            ws_.reset();  // 释放
        }
    }
    std::cout << "[" << name_ << "] Run loop exited" << std::endl;
}

void Connector::ping_loop() {
    std::cout << "[" << name_ << "] Ping loop started" << std::endl;
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (running_ && ws_) {
            // beast::error_code ec;
            boost::system::error_code ec;
            // ws_->ping("", ec);  // <--- 加 error_code
            // --- 修改处开始 ---
            // 不要使用 ws_->ping("")，Bitget 往往需要文本消息
            ws_->text(true); // 确保设置为文本模式
            ws_->write(boost::asio::buffer(std::string("ping")), ec);
            // --- 修改处结束 ---

            if (ec) {
                std::cerr << "[" << name_ << "] Ping failed: " << ec.message() << std::endl;
            } else {
                std::cout << "[" << name_ << "] Sent ping" << std::endl;
            }
        }
    }
    std::cout << "[" << name_ << "] Ping loop exited" << std::endl;
}
