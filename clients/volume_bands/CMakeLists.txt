add_executable(client_volume_bands client_volume_bands.cpp)  # 或 src/client_ volume_bands.cpp

target_include_directories(client_volume_bands PRIVATE
    ${CMAKE_BINARY_DIR}/generated
    /usr/local/include  # nlohmann/json.hpp
)

target_link_libraries(client_volume_bands PRIVATE
    proto_gen
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)