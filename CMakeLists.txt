cmake_minimum_required(VERSION 3.20)
project(RepoAggregator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

list(APPEND CMAKE_PREFIX_PATH "/usr/local")

# 强制优先找 /usr/local (源编译 gRPC/Protobuf 在这里)
set(CMAKE_PREFIX_PATH "/usr/local" ${CMAKE_PREFIX_PATH})

# 查找依赖
# find_package(Protobuf REQUIRED)
# find_package(gRPC REQUIRED)
# find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
# find_package(absl REQUIRED)  # <--- 关键：显式找 Abseil

find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

# 3. 查找依赖项
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
# 核心：必须以 CONFIG 模式查找，以获取准确的依赖链
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(nlohmann_json REQUIRED)

# 如果你手动安装了 Abseil，建议也显式引入
find_package(absl CONFIG REQUIRED)



# 子目录
add_subdirectory(proto)       # 生成 proto_gen 库
add_subdirectory(aggregator)  # 服务
add_subdirectory(clients)     # 三个客户端