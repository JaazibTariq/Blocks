#include "ConnectionHandler.h"
#include "Crypto.h"
#include "KVStore.h"
#include "Node.h"
#include "Protocol.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

ConnectionHandler::ConnectionHandler(KVStore& store, const std::string& enc_key, Node* node)
    : store_(store), enc_key_(enc_key), node_(node) {}

void ConnectionHandler::handle(int client_fd) {
    std::string buffer;
    char chunk[protocol::MAX_BUF];

    for (;;) {
        ssize_t n = recv(client_fd, chunk, sizeof(chunk), 0);
        if (n <= 0) break;

        buffer.append(chunk, n);

        std::string::size_type pos;
        while ((pos = buffer.find(protocol::DELIMITER)) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (line.empty()) continue;

            process_line(line, client_fd);
        }
    }

    close(client_fd);
}

void ConnectionHandler::process_line(const std::string& line, int client_fd) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == protocol::CMD_SET) {
        std::string key;
        iss >> key;
        // Value is everything after the second space
        std::string value;
        if (std::getline(iss >> std::ws, value) && !key.empty()) {
            std::string encrypted = Crypto::encrypt(value, enc_key_);
            store_.set(key, encrypted);
            send_response(client_fd, std::string(protocol::RSP_OK) + "\n");
        } else {
            send_response(client_fd, std::string(protocol::RSP_ERROR) + " usage: SET <key> <value>\n");
        }

    } else if (cmd == protocol::CMD_GET) {
        std::string key;
        iss >> key;
        if (key.empty()) {
            send_response(client_fd, std::string(protocol::RSP_ERROR) + " usage: GET <key>\n");
            return;
        }
        auto val = store_.get(key);
        if (val) {
            std::string plaintext = Crypto::decrypt(*val, enc_key_);
            send_response(client_fd, std::string(protocol::RSP_VALUE) + " " + plaintext + "\n");
        } else if (node_) {
            std::string forwarded = node_->forward_get(key);
            send_response(client_fd, forwarded + "\n");
        } else {
            send_response(client_fd, std::string(protocol::RSP_NOT_FOUND) + "\n");
        }

    } else if (cmd == protocol::CMD_FWD_GET) {
        // Forwarded GET — only check local store, never re-forward
        std::string key;
        iss >> key;
        if (key.empty()) {
            send_response(client_fd, std::string(protocol::RSP_ERROR) + " usage: FWDGET <key>\n");
            return;
        }
        auto val = store_.get(key);
        if (val) {
            std::string plaintext = Crypto::decrypt(*val, enc_key_);
            send_response(client_fd, std::string(protocol::RSP_VALUE) + " " + plaintext + "\n");
        } else {
            send_response(client_fd, std::string(protocol::RSP_NOT_FOUND) + "\n");
        }

    } else if (cmd == protocol::CMD_DELETE) {
        std::string key;
        iss >> key;
        if (key.empty()) {
            send_response(client_fd, std::string(protocol::RSP_ERROR) + " usage: DELETE <key>\n");
            return;
        }
        if (store_.del(key)) {
            send_response(client_fd, std::string(protocol::RSP_OK) + "\n");
        } else {
            send_response(client_fd, std::string(protocol::RSP_NOT_FOUND) + "\n");
        }

    } else if (cmd == protocol::CMD_HELLO) {
        std::string peer_addr;
        iss >> peer_addr;
        if (node_ && !peer_addr.empty()) {
            std::string response = node_->handle_hello(peer_addr);
            send_response(client_fd, response + "\n");
        } else {
            send_response(client_fd, std::string(protocol::RSP_ERROR) + " no P2P node configured\n");
        }

    } else if (cmd == protocol::CMD_QUIT) {
        // Client requested disconnect — handled by breaking the read loop
        return;

    } else {
        send_response(client_fd, std::string(protocol::RSP_ERROR) + " unknown command\n");
    }
}

void ConnectionHandler::send_response(int fd, const std::string& msg) {
    std::size_t total = 0;
    while (total < msg.size()) {
        ssize_t n = send(fd, msg.data() + total, msg.size() - total, MSG_NOSIGNAL);
        if (n <= 0) break;
        total += n;
    }
}
