cmake_minimum_required(VERSION 3.16)
project(RedisLiteX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(redislitex
    src/main.cpp
    src/protocol/resp.cpp
    src/store/kv_store.cpp
    src/server/tcp_server.cpp
)

target_include_directories(redislitex PRIVATE include)