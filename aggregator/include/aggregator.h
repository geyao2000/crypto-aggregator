#pragma once


#include "aggregator.pb.h"
#include "aggregator.grpc.pb.h"
// #include "../proto/aggregator.pb.h"
// #include "../proto/aggregator.grpc.pb.h"
// #include <grpcpp/server.h>
#include <grpcpp/grpcpp.h>
// #include <grpcpp/server_builder.h>
#include <map>
#include <mutex>
#include <vector>
#include <memory>
#include <string>

#include "binance_connector.h"
#include "okx_connector.h"
#include "bitget_connector.h"
#include "bybit_connector.h"

class Connector;  // 前向声明

class AggregatorServiceImpl final : public aggregator::AggregatorService::Service {
public:
    grpc::Status SubscribeBook(grpc::ServerContext* context,
                               const aggregator::SubscribeRequest* request,
                               grpc::ServerWriter<aggregator::BookUpdate>* writer) override;

    void add_subscriber(grpc::ServerWriter<aggregator::BookUpdate>* writer);
    void remove_subscriber(grpc::ServerWriter<aggregator::BookUpdate>* writer);
    void notify_all(const aggregator::BookUpdate& update);

private:
    std::vector<grpc::ServerWriter<aggregator::BookUpdate>*> subscribers_;
    std::mutex subscribers_mutex_;
};

class Aggregator {
public:
    Aggregator();
    ~Aggregator();

    void run_server();


    void on_book_updated(Connector* connector);
private:  
    void merge_books();

    std::unique_ptr<BinanceConnector> binance_;
    std::unique_ptr<OKXConnector> okx_;
    std::unique_ptr<BitgetConnector> bitget_;
    std::unique_ptr<BybitConnector> bybit_;

    std::map<double, double, std::greater<double>> consolidated_bids_;
    std::map<double, double> consolidated_asks_;
    std::mutex consolidated_mutex_;

    AggregatorServiceImpl service_;
    std::unique_ptr<grpc::Server> grpc_server_;
};