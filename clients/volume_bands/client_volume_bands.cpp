// client_volume_bands.cpp - Modified: Special output for 1k/10k/100k USD bands + full depth print
#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <ctime>
#include <map>

// int main() {

int main(int argc, char** argv) {
    std::string target_str = "localhost:50051";
    if (argc > 1) {
        target_str = argv[1];
    }
    std::cout << "Connecting to: " << target_str << std::endl;
    auto channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    auto stub = aggregator::AggregatorService::NewStub(channel);

    // auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    // auto stub = aggregator::AggregatorService::NewStub(channel);

    grpc::ClientContext context;
    aggregator::SubscribeRequest request;
    auto reader = stub->SubscribeBook(&context, request);

    aggregator::BookUpdate update;
    std::cout << std::fixed << std::setprecision(2);

    const std::vector<double> bands = {10000.0, 100000.0, 1000000.0, 5000000.0, 10000000.0, 25000000.0, 50000000.0};
    const std::vector<double> special_bands = {10000.0, 100000.0};  // 1k, 10k, 100k USD

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

        std::cout << "\n=== Volume Bands Update @ "
                  << std::put_time(&jst_tm, "%Y-%m-%d %H:%M:%S")
                  << '.' << std::setfill('0') << std::setw(3) << ms << " JST ===\n";

        // Maps to record reached price for specific bands
        std::map<double, double> reached_bid;  // band -> price
        std::map<double, double> reached_ask;

        auto print_bands = [&](const auto& levels, bool is_bid, auto& reached_map) {
            double cum_notional = 0.0;
            std::vector<double> remaining_bands = bands;

            for (int i = 0; i < levels.size(); ++i) {
                const auto& lvl = levels.Get(i);
                cum_notional += lvl.price() * lvl.quantity();

                remaining_bands.erase(
                    std::remove_if(remaining_bands.begin(), remaining_bands.end(),
                        [&](double band) {
                            if (cum_notional >= band) {
                                std::cout << (is_bid ? "Bid " : "Ask ") << band / 1000000.0 << "M USD @ "
                                          << lvl.price() << " (cum: " << cum_notional << ")\n";

                                // Record for special bands
                                if (std::find(special_bands.begin(), special_bands.end(), band) != special_bands.end()) {
                                    reached_map[band] = lvl.price();
                                }
                                return true;
                            }
                            return false;
                        }),
                    remaining_bands.end()
                );
            }

            double last_price = levels.size() > 0 ? levels.Get(levels.size() - 1).price() : 0.0;
            for (double band : remaining_bands) {
                std::cout << (is_bid ? "Bid " : "Ask ") << band / 1000000.0 << "M USD: not reached "
                          << "(cum: " << cum_notional << ", nearest @ " << last_price << ")\n";
            }
        };

        std::cout << "Bid Side:\n";
        print_bands(update.bids(), true, reached_bid);

        std::cout << "\nAsk Side:\n";
        print_bands(update.asks(), false, reached_ask);

        // Special output if 1k/10k/100k reached on BOTH sides
        bool all_special_reached = true;
        for (double band : special_bands) {
            if (reached_bid.find(band) == reached_bid.end() || reached_ask.find(band) == reached_ask.end()) {
                all_special_reached = false;
                break;
            }
        }

        if (all_special_reached) {
            std::cout << "\n*** Special 1k/10k/100k USD Bands (Both Sides Reached) ***\n";
            for (double band : special_bands) {
                std::cout << band / 1000.0 << "k USD: "
                          << "Bid @ " << reached_bid[band]
                          << " | Ask @ " << reached_ask[band] << "\n";
            }
            std::cout << "*********************************************************\n";
        }
        if(0){
            // Print full received depth (entire order book)
            std::cout << "\n--- Full Received Depth (Bids - high to low) ---\n";
            std::cout << std::setprecision(10);
            for (int i = 0; i < update.bids_size(); ++i) {
                const auto& lvl = update.bids(i);
                std::cout << "Bid " << i+1 << ": " << lvl.price() << " @ " << lvl.quantity() << "\n";
            }

            std::cout << "\n--- Full Received Depth (Asks - low to high) ---\n";
            for (int i = 0; i < update.asks_size(); ++i) {
                const auto& lvl = update.asks(i);
                std::cout << "Ask " << i+1 << ": " << lvl.price() << " @ " << lvl.quantity() << "\n";
            }

            std::cout << "==================================================\n";
        }
    }

    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "RPC failed: " << status.error_message() << std::endl;
    }
    return 0;
}