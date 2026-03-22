#include "Server.h"
#include "Protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

Server::Server(uint16_t port, std::size_t pool_size, HandlerFactory handler)
    : port_(port), pool_(pool_size), handler_(std::move(handler)) {}

Server::~Server() {
    stop();
}

void Server::start() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listen_fd_);
        throw std::runtime_error("bind() failed on port " + std::to_string(port_));
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        close(listen_fd_);
        throw std::runtime_error("listen() failed");
    }

    running_ = true;
    std::cout << "[server] listening on port " << port_ << std::endl;

    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (!running_) break;  // stop() closed the socket
            std::cerr << "[server] accept() error: " << strerror(errno) << std::endl;
            continue;
        }

        auto handler = handler_;
        pool_.enqueue([handler, client_fd]() {
            handler(client_fd);
        });
    }
}

void Server::stop() {
    if (running_.exchange(false)) {
        if (listen_fd_ >= 0) {
            shutdown(listen_fd_, SHUT_RDWR);
            close(listen_fd_);
            listen_fd_ = -1;
        }
    }
}
