#pragma once

#include "ThreadPool.h"

#include <atomic>
#include <cstdint>
#include <functional>

class KVStore;
class Node;

class Server {
public:
    using HandlerFactory = std::function<void(int client_fd)>;

    Server(uint16_t port, std::size_t pool_size, HandlerFactory handler);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start();   // blocking — runs accept loop
    void stop();

private:
    uint16_t port_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    ThreadPool pool_;
    HandlerFactory handler_;
};
