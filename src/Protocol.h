#pragma once

#include <cstddef>
#include <cstdint>

namespace protocol {

constexpr uint16_t DEFAULT_PORT   = 5000;
constexpr std::size_t MAX_BUF     = 4096;
constexpr char DELIMITER          = '\n';

// Client commands
constexpr const char* CMD_SET     = "SET";
constexpr const char* CMD_GET     = "GET";
constexpr const char* CMD_DELETE  = "DELETE";
constexpr const char* CMD_QUIT    = "QUIT";

// Peer commands
constexpr const char* CMD_HELLO   = "HELLO";
constexpr const char* CMD_PEERS   = "PEERS";

// Internal forwarding — distinguishes forwarded GETs to prevent infinite loops
constexpr const char* CMD_FWD_GET = "FWDGET";

// Responses
constexpr const char* RSP_OK        = "OK";
constexpr const char* RSP_VALUE     = "VALUE";
constexpr const char* RSP_NOT_FOUND = "NOT_FOUND";
constexpr const char* RSP_ERROR     = "ERROR";

} // namespace protocol
