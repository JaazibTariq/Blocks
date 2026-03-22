#include "Node.h"
#include "Protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

Node::Node(const std::string& self_addr) : self_addr_(self_addr) {}

void Node::add_peer(const std::string& addr) {
    if (addr == self_addr_) return;
    std::lock_guard lock(peers_mutex_);
    if (std::find(peers_.begin(), peers_.end(), addr) == peers_.end()) {
        peers_.push_back(addr);
        std::cout << "[node] added peer " << addr << std::endl;
    }
}

std::vector<std::string> Node::get_peers() const {
    std::lock_guard lock(peers_mutex_);
    return peers_;
}

bool Node::parse_address(const std::string& addr, std::string& host, uint16_t& port) {
    auto colon = addr.rfind(':');
    if (colon == std::string::npos) return false;
    host = addr.substr(0, colon);
    try {
        port = static_cast<uint16_t>(std::stoi(addr.substr(colon + 1)));
    } catch (...) {
        return false;
    }
    return true;
}

std::string Node::send_and_receive(const std::string& addr, const std::string& message) {
    std::string host;
    uint16_t port;
    if (!parse_address(addr, host, port))
        return std::string(protocol::RSP_ERROR) + " bad address";

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return std::string(protocol::RSP_ERROR) + " socket failed";

    // Set a 2-second connect/read timeout so forwarding doesn't hang
    struct timeval tv{};
    tv.tv_sec = 2;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in saddr{};
    saddr.sin_family = AF_INET;
    saddr.sin_port   = htons(port);
    inet_pton(AF_INET, host.c_str(), &saddr.sin_addr);

    if (connect(fd, reinterpret_cast<sockaddr*>(&saddr), sizeof(saddr)) < 0) {
        close(fd);
        return std::string(protocol::RSP_ERROR) + " connect failed";
    }

    std::string msg = message + "\n";
    send(fd, msg.data(), msg.size(), MSG_NOSIGNAL);

    std::string response;
    char buf[protocol::MAX_BUF];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, n);
        if (response.find('\n') != std::string::npos) break;
    }

    close(fd);

    // Strip trailing newline
    while (!response.empty() && (response.back() == '\n' || response.back() == '\r'))
        response.pop_back();

    return response;
}

void Node::join(const std::string& seed_addr) {
    std::cout << "[node] joining network via seed " << seed_addr << std::endl;

    add_peer(seed_addr);

    std::string hello_msg = std::string(protocol::CMD_HELLO) + " " + self_addr_;
    std::string response = send_and_receive(seed_addr, hello_msg);

    // Parse PEERS response: "PEERS addr1,addr2,..."
    if (response.rfind(protocol::CMD_PEERS, 0) == 0 && response.size() > 6) {
        std::string list = response.substr(6);  // skip "PEERS "
        std::istringstream ss(list);
        std::string peer;
        while (std::getline(ss, peer, ',')) {
            if (!peer.empty()) {
                add_peer(peer);
                // Announce ourselves to each discovered peer
                if (peer != seed_addr) {
                    send_and_receive(peer, hello_msg);
                }
            }
        }
    }

    auto peers = get_peers();
    std::cout << "[node] connected to network with " << peers.size() << " peer(s)" << std::endl;
}

std::string Node::handle_hello(const std::string& peer_addr) {
    add_peer(peer_addr);

    // Build PEERS response with our full peer list
    std::lock_guard lock(peers_mutex_);
    std::string response = std::string(protocol::CMD_PEERS) + " ";
    for (std::size_t i = 0; i < peers_.size(); ++i) {
        if (i > 0) response += ",";
        response += peers_[i];
    }
    // Include self in the peer list so the new node knows about us
    if (!peers_.empty()) response += ",";
    response += self_addr_;
    return response;
}

std::string Node::forward_get(const std::string& key) {
    auto peers = get_peers();
    for (const auto& peer : peers) {
        std::string fwd = std::string(protocol::CMD_FWD_GET) + " " + key;
        std::string response = send_and_receive(peer, fwd);
        if (response.rfind(protocol::RSP_VALUE, 0) == 0) {
            return response;
        }
    }
    return std::string(protocol::RSP_NOT_FOUND);
}
