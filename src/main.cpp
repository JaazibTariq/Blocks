#include "ConnectionHandler.h"
#include "KVStore.h"
#include "Node.h"
#include "Protocol.h"
#include "Server.h"

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

static Server* g_server = nullptr;

static void signal_handler(int) {
    if (g_server) g_server->stop();
}

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --port <port> --key <passphrase>"
              << " [--threads <N>] [--seed <ip:port>]\n";
}

int main(int argc, char* argv[]) {
    uint16_t port       = protocol::DEFAULT_PORT;
    std::size_t threads = 4;
    std::string enc_key;
    std::string seed_addr;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--threads" && i + 1 < argc) {
            threads = static_cast<std::size_t>(std::stoi(argv[++i]));
        } else if (arg == "--key" && i + 1 < argc) {
            enc_key = argv[++i];
        } else if (arg == "--seed" && i + 1 < argc) {
            seed_addr = argv[++i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (enc_key.empty()) {
        std::cerr << "Error: --key is required\n";
        print_usage(argv[0]);
        return 1;
    }

    KVStore store;
    std::string self_addr = "127.0.0.1:" + std::to_string(port);
    Node node(self_addr);

    Server server(port, threads, [&store, &enc_key, &node](int client_fd) {
        ConnectionHandler handler(store, enc_key, &node);
        handler.handle(client_fd);
    });

    g_server = &server;
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!seed_addr.empty()) {
        node.join(seed_addr);
    }

    std::cout << "[main] p2p-kvstore starting on port " << port
              << " with " << threads << " worker threads" << std::endl;

    server.start();

    std::cout << "[main] shutting down" << std::endl;
    return 0;
}
