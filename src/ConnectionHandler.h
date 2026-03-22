#pragma once

#include <string>

class KVStore;
class Node;

class ConnectionHandler {
public:
    ConnectionHandler(KVStore& store, const std::string& enc_key, Node* node);

    void handle(int client_fd);

private:
    void process_line(const std::string& line, int client_fd);
    void send_response(int fd, const std::string& msg);
    std::string read_line_from(int fd);

    KVStore& store_;
    std::string enc_key_;
    Node* node_;  // nullable — set to nullptr before P2P integration
};
