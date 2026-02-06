#include "aggregator.h"
#include "binance_connector.h"
#include "okx_connector.h"
#include "bitget_connector.h"
#include <iostream>
#include <thread>
#include <chrono>

Aggregator::Aggregator() {
    std::cout << "Aggregator constructed, creating connectors..." << std::endl;
    binance_ = std::make_unique<BinanceConnector>(this);
    okx_ = std::make_unique<OKXConnector>(this);
    bitget_ = std::make_unique<BitgetConnector>(this);
    bybit_ = std::make_unique<BybitConnector>(this);
    std::cout << "Connectors created" << std::endl;
}

Aggregator::~Aggregator() = default;

void Aggregator::run_server() {
    std::cout << "Starting connectors..." << std::endl;
    binance_->start();  //level = 5; tz = 0.01 snap
    okx_->start();   // level = 5; tz = 0.1; snap
    bitget_->start();   // level = 39; tz = 0.01 snap
    bybit_->start(); //level = 50; tz = 0.1; update new
    std::cout << "Connectors started" << std::endl;

    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    grpc_server_ = builder.BuildAndStart();

    std::cout << "Aggregator gRPC server running on port 50051" << std::endl;
    grpc_server_->Wait();
}

void Aggregator::on_book_updated(Connector* connector) {
    // std::cout << "[Aggregator] on_book_updated called from " << connector->name_ << std::endl;
    merge_books();
}

// void Aggregator::merge_books() {
//     std::lock_guard<std::mutex> lock(consolidated_mutex_);
//     consolidated_bids_.clear();
//     consolidated_asks_.clear();

//     auto merge = [](auto& target, const auto& source) {
//         for (const auto& [price, qty] : source) {
//             target[price] += qty;
//         }
//     };
    
//     merge(consolidated_bids_, binance_->get_bids_snapshot());
//     merge(consolidated_asks_, binance_->get_asks_snapshot());

//     merge(consolidated_bids_, okx_->get_bids_snapshot());
//     merge(consolidated_asks_, okx_->get_asks_snapshot());

//     merge(consolidated_bids_, bitget_->get_bids_snapshot());
//     merge(consolidated_asks_, bitget_->get_asks_snapshot());

//     merge(consolidated_bids_, bybit_->get_bids_snapshot());
//     merge(consolidated_asks_, bybit_->get_asks_snapshot());

//     aggregator::BookUpdate update;
//     update.set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
//         std::chrono::system_clock::now().time_since_epoch()).count());

//     // 通用 lambda：填充 bids 或 asks（限制 max_levels）
//     auto fill_levels = [&](auto& consolidated, auto add_func, int max_levels) {
//         int count = 0;
//         for (const auto& [p, q] : consolidated) {
//             if (count++ >= max_levels) break;
//             auto* lvl = add_func(&update);
//             lvl->set_price(p);
//             lvl->set_quantity(q);
//         }
//     };
   
//     fill_levels(consolidated_bids_, [](auto* u) { return u->add_bids(); }, 100);
//     fill_levels(consolidated_asks_, [](auto* u) { return u->add_asks(); }, 100);

//     // 打印 Top 10（复用相同逻辑，避免重复代码）
//     // std::cout << "\n>>> 合并后 Consolidated Book (Top 10) <<<\n";
//     // std::cout << std::fixed << std::setprecision(2);
//     bool if_print = 1;
//     if(if_print){
//         std::cout<<"-----------new--"<<std::endl;
//         int max_lv = 50;
//         int print_count = 0;  // 用不同的变量名，避免和上面冲突
        
//         for (const auto& [p, q] : consolidated_bids_) {
//             if (print_count ++ >= max_lv) break;
//             std::cout << "Bid:"<<max_lv - print_count + 1 << std::setw(12) << p << " @ " << q << "\n";
//         }
//         print_count = 0;
        
//         for (const auto& [p, q] : consolidated_asks_) {
//             if (print_count++ >= max_lv) break;
//             std::cout << "Ask:"<<print_count<< std::setw(12) << p << " @ " << q << "\n";
//         }
        
//     }
//     // std::cout << ">>>\n\n";
//     service_.notify_all(update);
// }

void Aggregator::merge_books() {
    std::lock_guard<std::mutex> lock(consolidated_mutex_);
    consolidated_bids_.clear();
    consolidated_asks_.clear();

    auto merge = [](auto& target, const auto& source) {
        for (const auto& [price, qty] : source) {
            target[price] += qty;
        }
    };

    merge(consolidated_bids_, binance_->get_bids_snapshot());
    merge(consolidated_asks_, binance_->get_asks_snapshot());

    // 如果启用其他 connector，取消注释并使用 snapshot
    merge(consolidated_bids_, okx_->get_bids_snapshot());
    merge(consolidated_asks_, okx_->get_asks_snapshot());
    merge(consolidated_bids_, bitget_->get_bids_snapshot());
    merge(consolidated_asks_, bitget_->get_asks_snapshot());
    merge(consolidated_bids_, bybit_->get_bids_snapshot());
    merge(consolidated_asks_, bybit_->get_asks_snapshot());

    aggregator::BookUpdate update;
    update.set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    // 填充 bids (top 100 for gRPC)
    int count = 0;
    for (const auto& [price, qty] : consolidated_bids_) {
        if (count++ >= 100) break;
        auto* lvl = update.add_bids();
        lvl->set_price(price);
        lvl->set_quantity(qty);
    }

    // 填充 asks (top 100 for gRPC)
    count = 0;
    for (const auto& [price, qty] : consolidated_asks_) {
        if (count++ >= 100) break;
        auto* lvl = update.add_asks();
        lvl->set_price(price);
        lvl->set_quantity(qty);
    }

    if(0){
        // <--- 新增：打印完整合并深度（所有层级，无 top 限制）
        std::cout << "\n>>> 合并后 Consolidated Book (Full Depth) <<<\n";
        std::cout << std::fixed << std::setprecision(2);  // 价格2位
        std::cout << "Bids (买盘 - 高到低, 共 " << consolidated_bids_.size() << " 层):\n";
        int idx = 1;
        for (const auto& [p, q] : consolidated_bids_) {
            std::cout << std::setw(3) << idx++ << ": " << std::setw(12);
            printf("bid: %10.1f,",p);
            std::cout << " @ " << std::setprecision(10);
            printf("%.8f\n",q);
        }

        std::cout << "\nAsks (卖盘 - 低到高, 共 " << consolidated_asks_.size() << " 层):\n";
        idx = 1;
        for (const auto& [p, q] : consolidated_asks_) {
            std::cout << std::setw(3) << idx++ << ": " << std::setw(12);
            printf("ask: %10.1f,",p);
            std::cout<< " @ " << std::setprecision(10);
            printf("%.8f\n",q);
        }
        std::cout << ">>> End of Full Depth <<<\n\n";
    }
    service_.notify_all(update);
}

// AggregatorServiceImpl 实现
grpc::Status AggregatorServiceImpl::SubscribeBook(grpc::ServerContext* context,
                                                 const aggregator::SubscribeRequest*,
                                                 grpc::ServerWriter<aggregator::BookUpdate>* writer) {
    add_subscriber(writer);
    while (!context->IsCancelled()) std::this_thread::sleep_for(std::chrono::seconds(1));
    remove_subscriber(writer);
    return grpc::Status::OK;
}

void AggregatorServiceImpl::add_subscriber(grpc::ServerWriter<aggregator::BookUpdate>* w) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_.push_back(w);
}

void AggregatorServiceImpl::remove_subscriber(grpc::ServerWriter<aggregator::BookUpdate>* w) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_.erase(std::remove(subscribers_.begin(), subscribers_.end(), w), subscribers_.end());
}

void AggregatorServiceImpl::notify_all(const aggregator::BookUpdate& update) {
    // std::cout << "notify_all called! Pushing update with " 
    //           << update.bids_size() << " bids / " << update.asks_size() << " asks to " 
    //           << subscribers_.size() << " subscribers" << std::endl;

    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    for (auto it = subscribers_.begin(); it != subscribers_.end(); ) {
        if (!(*it)->Write(update)) it = subscribers_.erase(it);
        else ++it;
    }
    // std::cout<<"test3"<<std::endl;
}