#pragma once

#include <mutex>
#include <string>
#include <vector>

class Node {
public:
    explicit Node(const std::string& self_addr);

    void join(const std::string& seed_addr);
    std::string handle_hello(const std::string& peer_addr);
    std::string forward_get(const std::string& key);

    std::vector<std::string> get_peers() const;

private:
    void add_peer(const std::string& addr);
    std::string send_and_receive(const std::string& addr, const std::string& message);
    static bool parse_address(const std::string& addr, std::string& host, uint16_t& port);

    std::string self_addr_;
    std::vector<std::string> peers_;
    mutable std::mutex peers_mutex_;
};
