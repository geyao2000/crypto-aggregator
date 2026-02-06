#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <vector>
#include <limits>

// int main() {

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

    const std::vector<int> bps_levels = {1,2,5,10,20,50, 100, 200, 500, 1000};  // bps

    while (reader->Read(&update)) {
        // Convert timestamp_ms to JST (UTC+9)
        auto timestamp_ms = update.timestamp_ms();
        auto tp = std::chrono::milliseconds(timestamp_ms);
        auto time_point = std::chrono::time_point<std::chrono::system_clock>(tp);

        auto jst_offset = std::chrono::hours(9);
        auto jst_time_point = time_point + jst_offset;

        std::time_t tt = std::chrono::system_clock::to_time_t(jst_time_point);
        std::tm jst_tm = *std::localtime(&tt);
        auto ms = timestamp_ms % 1000;

        std::cout << "\n=== Price Bands Update @ "
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

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "BBO: Best Bid " << best_bid
                  << " | Best Ask " << best_ask
                  << " | Mid " << mid << "\n\n";

        for (int bps : bps_levels) {
            double bps_offset = mid * (bps / 10000.0);

            double target_bid = mid - bps_offset;  // 更低的买价
            double target_ask = mid + bps_offset;  // 更高的卖价

            // Find closest bid: highest price <= target_bid
            double bid_price = 0.0;
            double bid_qty = 0.0;
            for (int i = 0; i < update.bids_size(); ++i) {
                double p = update.bids(i).price();
                if (p <= target_bid + 1e-6) {  // 容差
                    bid_price = p;
                    bid_qty = update.bids(i).quantity();
                    break;  // bids 已从高到低，第一匹配就是最近
                }
            }

            // Find closest ask: lowest price >= target_ask
            double ask_price = 0.0;
            double ask_qty = 0.0;
            for (int i = 0; i < update.asks_size(); ++i) {
                double p = update.asks(i).price();
                if (p >= target_ask - 1e-6) {
                    ask_price = p;
                    ask_qty = update.asks(i).quantity();
                    break;  // asks 已从低到高，第一匹配就是最近
                }
            }

            std::cout << "+" << std::setw(4) << bps << " bps: "
                      << "Bid " << (bid_price > 0.0 ? std::to_string(bid_price) : "N/A")
                      << " @ " << std::setprecision(10) << (bid_price > 0.0 ? bid_qty : 0.0)
                      << " | Ask " << (ask_price > 0.0 ? std::to_string(ask_price) : "N/A")
                      << " @ " << (ask_price > 0.0 ? ask_qty : 0.0) << "\n";
        }

        std::cout << "\n==============================================\n";
    }

    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "RPC failed: " << status.error_message() << std::endl;
    }
    return 0;

}
