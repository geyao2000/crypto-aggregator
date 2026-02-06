// #include "okx_connector.h"
// #include "binance_connector.h"
// #include "bitget_connector.h"
// #include <iostream>
// #include <thread>
// #include <chrono>
#include "aggregator.h"

using namespace std;


int main() {
    Aggregator agg;
    agg.run_server();
    return 0;
}

// int main() {
//     cout << "同時接收 OKX 和 Binance BTCUSDT 行情，按 Ctrl+C 退出..." << endl;

//     OKXConnector okx_conn;
//     okx_conn.start();
    
//     BinanceConnector bnc_conn;
//     bnc_conn.start();

//     BitgetConnector bitget_conn;
//     bitget_conn.start();
//     // BybitConnector bybit_conn;
//     // bybit_conn.start();

//     // KrakenConnector kraken_conn;
//     // kraken_conn.start();

//     // CoinbaseConnector coinbase_conn;
//     // coinbase_conn.start();

//     // 主線程保持運行
//     while (true) {
//         this_thread::sleep_for(chrono::hours(1));
//     }

//     return 0;
// }

