add_executable(client_bbo client_bbo.cpp)  # 或 src/client_bbo.cpp

target_include_directories(client_bbo PRIVATE
    ${CMAKE_BINARY_DIR}/generated
    /usr/local/include  # nlohmann/json.hpp
)

target_link_libraries(client_bbo PRIVATE
    proto_gen
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)