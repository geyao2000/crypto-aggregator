#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>

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

    while (reader->Read(&update)) {
        // Convert timestamp_ms (milliseconds since Unix epoch) to readable time
        auto timestamp_ms = update.timestamp_ms();
        auto tp = std::chrono::milliseconds(timestamp_ms);
        auto time_point = std::chrono::time_point<std::chrono::system_clock>(tp);
        std::time_t tt = std::chrono::system_clock::to_time_t(time_point);
        std::tm local_tm = *std::localtime(&tt);  // Local time (your system timezone, e.g., JST if set)
        auto ms = timestamp_ms % 1000;

        std::cout << "\n=== BBO Update @ "
                  << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
                  << '.' << std::setfill('0') << std::setw(3) << ms << " (Local) ===\n";

        // Optional: UTC time
        // std::tm utc_tm = *std::gmtime(&tt);
        // std::cout << " (UTC: " << std::put_time(&utc_tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << ms << ")\n";

        if (update.bids_size() > 0) {
            const auto& best_bid = update.bids(0);
            std::cout << "Best Bid:  " 
                      << std::fixed << std::setprecision(2) << std::setw(12) << best_bid.price()
                      << " @ " << std::setprecision(10) << best_bid.quantity() << "\n";
        } else {
            std::cout << "Best Bid:  ---\n";
        }

        if (update.asks_size() > 0) {
            const auto& best_ask = update.asks(0);
            std::cout << "Best Ask:  " 
                      << std::fixed << std::setprecision(2) << std::setw(12) << best_ask.price()
                      << " @ " << std::setprecision(10) << best_ask.quantity() << "\n";
        } else {
            std::cout << "Best Ask:  ---\n";
        }

        std::cout << "========================================\n";
    }

    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "RPC failed: " << status.error_message() << std::endl;
    }
    return 0;

}
