cmake_minimum_required(VERSION 3.16)
project(p2p-kvstore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(p2p-kvstore
    src/main.cpp
    src/KVStore.cpp
    src/Crypto.cpp
    src/ThreadPool.cpp
    src/Server.cpp
    src/ConnectionHandler.cpp
    src/Node.cpp
)

target_include_directories(p2p-kvstore PRIVATE src)
target_link_libraries(p2p-kvstore PRIVATE OpenSSL::Crypto Threads::Threads)
