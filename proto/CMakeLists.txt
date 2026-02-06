# proto 生成库（生成文件到 build/generated）
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/aggregator.proto)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/../generated)  # build/generated
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(PROTO_SRCS ${GENERATED_DIR}/aggregator.pb.cc)
set(PROTO_HDRS ${GENERATED_DIR}/aggregator.pb.h)
set(GRPC_SRCS ${GENERATED_DIR}/aggregator.grpc.pb.cc)
set(GRPC_HDRS ${GENERATED_DIR}/aggregator.grpc.pb.h)

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND protobuf::protoc
    ARGS --cpp_out=${GENERATED_DIR}
         --grpc_out=${GENERATED_DIR}
         -I ${CMAKE_CURRENT_SOURCE_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating protobuf and gRPC sources"
    VERBATIM
)

add_library(proto_gen STATIC ${PROTO_SRCS} ${GRPC_SRCS})
target_include_directories(proto_gen PUBLIC ${GENERATED_DIR})
target_link_libraries(proto_gen PUBLIC protobuf::libprotobuf gRPC::grpc++)