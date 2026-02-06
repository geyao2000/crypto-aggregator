add_executable(client_price_bands client_price_bands.cpp)  # 或 src/client_ price_bands.cpp

target_include_directories(client_price_bands PRIVATE
    ${CMAKE_BINARY_DIR}/generated
    /usr/local/include  # nlohmann/json.hpp
)

target_link_libraries(client_price_bands PRIVATE
    proto_gen
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)