#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <vector>
#include <algorithm>  // std::min

int main(int argc, char** argv) {
    std::string target_str = "localhost:50051";
    if (argc > 1) {
        target_str = argv[1];
    }
    std::cout << "Connecting to: " << target_str << std::endl;

    auto channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    auto stub = aggregator::AggregatorService::NewStub(channel);

    grpc::ClientContext context;
    aggregator::SubscribeRequest request;
    auto reader = stub->SubscribeBook(&context, request);

    aggregator::BookUpdate update;
    std::cout << std::fixed << std::setprecision(2);

    // Volume Bands 阈值（USD）
    const std::vector<double> bands = {10000.0, 100000.0, 1000000.0, 5000000.0, 10000000.0, 25000000.0, 50000000.0};

    while (reader->Read(&update)) {
        // 时间戳转 JST (UTC+9)
        auto timestamp_ms = update.timestamp_ms();
        auto tp = std::chrono::milliseconds(timestamp_ms);
        auto time_point = std::chrono::time_point<std::chrono::system_clock>(tp);
        auto jst_offset = std::chrono::hours(9);
        auto jst_time_point = time_point + jst_offset;
        std::time_t tt = std::chrono::system_clock::to_time_t(jst_time_point);
        std::tm jst_tm = *std::localtime(&tt);
        auto ms = timestamp_ms % 1000;

        std::cout << "\n=== Volume Bands Update @ "
                  << std::put_time(&jst_tm, "%Y-%m-%d %H:%M:%S")
                  << '.' << std::setfill('0') << std::setw(3) << ms << " JST ===\n";

        double best_bid = update.bids_size() > 0 ? update.bids(0).price() : 0.0;
        double best_ask = update.asks_size() > 0 ? update.asks(0).price() : 0.0;

        if (best_bid <= 0.0 || best_ask <= 0.0) {
            std::cout << "No valid BBO\n";
            std::cout << "==============================================\n";
            continue;
        }

        double mid = (best_bid + best_ask) / 2.0;

        if(false){
            // 打印前 10 档行情 + 累计 notional
            std::cout << "Received Depth (Top 10 Bids - high to low):\n";
            double cum_bid_notional = 0.0;
            for (int i = 0; i < std::min(10, update.bids_size()); ++i) {
                const auto& lvl = update.bids(i);
                cum_bid_notional += lvl.price() * lvl.quantity();
                // std::cout << "  Bid " << i+1 << ": " << lvl.price() << " @ " << lvl.quantity()
                //           << "      Cumulative Notional: " << cum_bid_notional << " USD\n";
                // 高精度打印 quantity（15 位小数）
                std::cout << "  Bid " << i+1 << ": " << lvl.price() << " @ "
                        << std::setprecision(15) << lvl.quantity()  // <--- 加这一行
                        << std::fixed << std::setprecision(2) << "      Cumulative Notional: " << cum_bid_notional << " USD\n";
            }

            std::cout << "\nReceived Depth (Top 10 Asks - low to high):\n";
            double cum_ask_notional = 0.0;
            for (int i = 0; i < std::min(10, update.asks_size()); ++i) {
                const auto& lvl = update.asks(i);
                cum_ask_notional += lvl.price() * lvl.quantity();
                std::cout << "  Ask " << i+1 << ": " << lvl.price() << " @ " << lvl.quantity()
                        << "      Cumulative Notional: " << cum_ask_notional << " USD\n";
            }
        }

        // Volume Bands 计算函数（逐级检查）
        auto compute_volume_bands = [&](const auto& levels, bool is_bid) {
            double cum_notional = 0.0;
            std::vector<double> remaining = bands;

            for (int i = 0; i < levels.size(); ++i) {
                const auto& lvl = levels.Get(i);
                double level_notional = lvl.price() * lvl.quantity();
                cum_notional += level_notional;

                // 逐个检查每个剩余阈值
                for (auto it = remaining.begin(); it != remaining.end(); ) {
                    if (cum_notional >= *it) {
                        std::cout << (is_bid ? "Bid " : "Ask ") << *it / 1000000.0 << "M USD @ "
                                  << lvl.price() << " (cum: " << cum_notional << ")\n";
                        it = remaining.erase(it);
                    } else {
                        ++it;
                    }
                }

                if (remaining.empty()) break;
            }

            // 未达到的阈值
            for (double band : remaining) {
                double last_price = levels.size() > 0 ? levels.Get(levels.size() - 1).price() : 0.0;
                std::cout << (is_bid ? "Bid " : "Ask ") << band / 1000000.0 << "M USD: not reached "
                          << "(cum: " << cum_notional << ", nearest @ " << last_price << ")\n";
            }
        };

        std::cout << "\nVolume Bands:\n";
        compute_volume_bands(update.bids(), true);
        compute_volume_bands(update.asks(), false);

        std::cout << "==============================================\n";
    }

    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "RPC failed: " << status.error_message() << std::endl;
    }
    return 0;
}