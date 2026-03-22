# p2p-kvstore

A distributed, AES-256 encrypted key-value store built in C++17. Nodes form a peer-to-peer network via a gossip protocol, serve SET/GET/DELETE commands over TCP, and transparently encrypt all stored values using OpenSSL.

## Architecture

```
┌──────────┐     TCP      ┌──────────────────────────────────────────┐
│  Client   │────────────▶│  Server (accept loop + thread pool)      │
│ (netcat)  │             │                                          │
└──────────┘             │  ┌──────────────────┐                    │
                          │  │ ConnectionHandler │                    │
                          │  │  parse commands   │                    │
                          │  └───────┬──────────┘                    │
                          │          │                                │
                          │    ┌─────▼─────┐    ┌──────────┐         │
                          │    │   Crypto   │───▶│  KVStore  │         │
                          │    │ AES-256-CBC│    │ shared_   │         │
                          │    └───────────┘    │ mutex map │         │
                          │                      └──────────┘         │
                          │  ┌──────────────┐                        │
                          │  │     Node     │◀── gossip + forwarding  │
                          │  │  peer list   │                        │
                          │  └──────────────┘                        │
                          └──────────────────────────────────────────┘
                                     │
                              TCP    │    TCP
                          ┌──────────┼──────────┐
                          ▼          ▼          ▼
                       Node B     Node C     Node ...
```

**Request flow:** Client connects to any node via TCP. The `ConnectionHandler` parses the ASCII command, passes values through the `Crypto` layer (AES-256-CBC encrypt on SET, decrypt on GET), and reads/writes the thread-safe `KVStore`. On a GET miss, the `Node` component forwards the request to peers until a hit is found.

## Building

Requirements: CMake 3.16+, a C++17 compiler (GCC 8+ or Clang 7+), and OpenSSL development headers.

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt update && sudo apt install -y build-essential cmake libssl-dev

# Build
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

The binary is produced at `build/p2p-kvstore`.

## Usage

```bash
./p2p-kvstore --port <port> --key <passphrase> [--threads <N>] [--seed <ip:port>]
```

| Flag | Description | Default |
|------|-------------|---------|
| `--port` | TCP listen port | 5000 |
| `--key` | AES-256 encryption passphrase (required) | — |
| `--threads` | Thread pool worker count | 4 |
| `--seed` | Address of an existing node to join | — (seed mode) |

## Wire Protocol

Commands are newline-delimited ASCII, testable with `netcat`:

```
SET <key> <value>\n     →  OK\n
GET <key>\n             →  VALUE <data>\n  or  NOT_FOUND\n
DELETE <key>\n          →  OK\n  or  NOT_FOUND\n
```

Peer-internal commands:

```
HELLO <ip:port>\n       →  PEERS <ip:port>,<ip:port>,...\n
FWDGET <key>\n          →  VALUE <data>\n  or  NOT_FOUND\n
```

## Demo: 3-Node Cluster

Launch three nodes locally and test cross-node key-value operations:

```bash
# Terminal 1 — seed node
./build/p2p-kvstore --port 5000 --key "secret"

# Terminal 2 — joins via seed
./build/p2p-kvstore --port 5001 --key "secret" --seed 127.0.0.1:5000

# Terminal 3 — joins via seed
./build/p2p-kvstore --port 5002 --key "secret" --seed 127.0.0.1:5000
```

Now from a fourth terminal:

```bash
# Store a value on node A
echo "SET greeting hello-world" | nc -q1 127.0.0.1 5000
# OK

# Retrieve it from node C — forwarded across the network
echo "GET greeting" | nc -q1 127.0.0.1 5002
# VALUE hello-world

# Delete from node A
echo "DELETE greeting" | nc -q1 127.0.0.1 5000
# OK
```

Or run the automated demo script:

```bash
chmod +x scripts/demo.sh
./scripts/demo.sh
```

## File Structure

```
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp               Entry point, CLI parsing, signal handling
│   ├── Protocol.h             Wire format constants and command strings
│   ├── KVStore.h / .cpp       Thread-safe hash map (shared_mutex)
│   ├── Crypto.h / .cpp        AES-256-CBC encrypt/decrypt via OpenSSL
│   ├── ThreadPool.h / .cpp    Fixed-size worker thread pool
│   ├── Server.h / .cpp        TCP accept loop, dispatches to pool
│   ├── ConnectionHandler.h/.cpp  Command parsing and request routing
│   └── Node.h / .cpp          Peer discovery (gossip) and GET forwarding
└── scripts/
    └── demo.sh                Launches 3 nodes and runs test commands
```

## Technical Highlights

- **Concurrent reads**: `std::shared_mutex` allows multiple GET operations simultaneously without blocking each other.
- **AES-256-CBC encryption**: Every value is encrypted with a random IV before storage. The IV is prepended to the ciphertext and base64-encoded for safe ASCII transport.
- **Thread pool**: Fixed-size pool avoids thread-per-connection overhead. Uses `std::condition_variable` for efficient work distribution.
- **Gossip protocol**: Nodes discover each other through a seed node. HELLO/PEERS handshake propagates the peer list to all participants.
- **Forward-on-miss**: GET requests that miss locally are forwarded to peers via `FWDGET` (a distinct command that prevents infinite forwarding loops).
- **RAII throughout**: Sockets, cipher contexts, and threads are all cleaned up deterministically.

## License

This project was built as a portfolio demonstration for systems-level C++ development.
