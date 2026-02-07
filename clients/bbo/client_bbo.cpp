#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <thread>
#include <string>

int main(int argc, char** argv) {
    std::string target_str = "localhost:50051";
    if (argc > 1) {
        target_str = argv[1];
    }
    std::cout << "Connecting to: " << target_str << std::endl;

    const int max_retries = 10;
    const int base_backoff_ms = 100;  // 初始等待 100ms

    int retry_count = 0;

    while (true) {  // 外层重试循环
        auto channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
        auto stub = aggregator::AggregatorService::NewStub(channel);

        grpc::ClientContext context;
        aggregator::SubscribeRequest request;
        auto reader = stub->SubscribeBook(&context, request);

        aggregator::BookUpdate update;

        std::cout << std::fixed << std::setprecision(2);

        bool connected = false;

        while (reader->Read(&update)) {
            connected = true;
            retry_count = 0;  // 成功读取，重置重试计数

            // 时间戳转本地时间（JST UTC+9）
            auto timestamp_ms = update.timestamp_ms();
            auto tp = std::chrono::milliseconds(timestamp_ms);
            auto time_point = std::chrono::time_point<std::chrono::system_clock>(tp);
            auto jst_offset = std::chrono::hours(9);
            auto jst_time_point = time_point + jst_offset;
            std::time_t tt = std::chrono::system_clock::to_time_t(jst_time_point);
            std::tm jst_tm = *std::localtime(&tt);
            auto ms = timestamp_ms % 1000;

            std::cout << "\n=== BBO Update @ "
                      << std::put_time(&jst_tm, "%Y-%m-%d %H:%M:%S")
                      << '.' << std::setfill('0') << std::setw(3) << ms << " Local ===\n";
            
            const auto& best_ask = update.asks(0);
            const auto& best_bid = update.bids(0);
            if (update.asks_size() > 0) {
                std::cout << "Best Ask:  " ;
                printf("\033[34m%10.2f\033[0m @ %13.8f \n",best_ask.price(),best_ask.quantity());
            } else {
                std::cout << "Best Ask:  ---\n";
            }

            if (update.bids_size() > 0) {
                std::cout << "Best Bid:  ";
                printf("\033[31m%10.2f\033[0m @ %13.8f",best_bid.price(),best_bid.quantity());
                
                if(best_bid.price()>best_ask.price()){
                    double crossed = best_bid.price() - best_ask.price();
                    printf(",   warning: crossed: %5.2f",crossed);
                }
                std::cout<<std::endl;
            } else {
                std::cout << "Best Bid:  ---\n";
            }

            
            std::cout << "========================================\n";
        }

        grpc::Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_code() << ": " << status.error_message() << std::endl;
        }

        if (connected) {
            // 正常结束流，重试计数重置
            retry_count = 0;
        } else {
            // 连接失败或未连接，重试
            retry_count++;
            if (retry_count > max_retries) {
                std::cerr << "Max retries reached. Exiting.\n";
                break;
            }

            int backoff_ms = base_backoff_ms * (1 << (retry_count - 1));  // exponential backoff
            std::cout << "Connection lost. Retrying in " << backoff_ms << "ms (attempt " << retry_count << "/" << max_retries << ")...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
        }
    }

    return 0;
}