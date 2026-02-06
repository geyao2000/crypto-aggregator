file(GLOB_RECURSE SRC_FILES "src/*.cpp")

add_executable(aggregator ${SRC_FILES})

find_package(Boost COMPONENTS system REQUIRED)
find_package(nlohmann_json REQUIRED)
# find_package(gRPC CONFIG REQUIRED)

target_include_directories(aggregator PRIVATE
    include
    ${CMAKE_BINARY_DIR}/generated  # build/generated
    /usr/local/include  # nlohmann/json.hpp
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(aggregator PRIVATE
    proto_gen
    nlohmann_json::nlohmann_json
    gRPC::grpc++
    protobuf::libprotobuf
    
	absl::log_internal_message      # <--- 关键日志
    absl::log_internal_check_op
    absl::log_internal_log_sink_set
    absl::log_internal_proto
    absl::log_severity
    absl::log_globals
    absl::log_entry
    absl::log_sink
    absl::raw_logging_internal
    absl::log_internal_format
    absl::log_internal_nullguard
    absl::base
    absl::strings
    absl::str_format
    
	Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

set_target_properties(aggregator PROPERTIES
    INSTALL_RPATH "/usr/local/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)