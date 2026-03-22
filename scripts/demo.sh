#!/bin/bash
set -e

BINARY="${1:-./build/p2p-kvstore}"
KEY="demo-secret-key"

if [ ! -x "$BINARY" ]; then
    echo "Binary not found at $BINARY"
    echo "Build first:  mkdir -p build && cd build && cmake .. && make -j\$(nproc)"
    exit 1
fi

cleanup() {
    echo ""
    echo "=== Cleaning up ==="
    kill "$PID_A" "$PID_B" "$PID_C" 2>/dev/null || true
    wait "$PID_A" "$PID_B" "$PID_C" 2>/dev/null || true
}
trap cleanup EXIT

echo "=== Starting 3-node P2P cluster ==="
echo ""

echo "[1/3] Starting seed node A on port 5000..."
$BINARY --port 5000 --key "$KEY" &
PID_A=$!
sleep 1

echo "[2/3] Starting node B on port 5001 (joining via seed)..."
$BINARY --port 5001 --key "$KEY" --seed 127.0.0.1:5000 &
PID_B=$!
sleep 1

echo "[3/3] Starting node C on port 5002 (joining via seed)..."
$BINARY --port 5002 --key "$KEY" --seed 127.0.0.1:5000 &
PID_C=$!
sleep 1

echo ""
echo "=== All nodes running ==="
echo ""

echo "--- SET 'hello' = 'world' on node A (port 5000) ---"
echo "SET hello world" | nc -q1 127.0.0.1 5000
echo ""

echo "--- SET 'language' = 'C++17' on node B (port 5001) ---"
echo "SET language C++17" | nc -q1 127.0.0.1 5001
echo ""

echo "--- GET 'hello' from node C (port 5002) — should forward from A ---"
echo "GET hello" | nc -q1 127.0.0.1 5002
echo ""

echo "--- GET 'language' from node A (port 5000) — should forward from B ---"
echo "GET language" | nc -q1 127.0.0.1 5000
echo ""

echo "--- GET missing key 'nonexistent' from node C ---"
echo "GET nonexistent" | nc -q1 127.0.0.1 5002
echo ""

echo "--- DELETE 'hello' from node A ---"
echo "DELETE hello" | nc -q1 127.0.0.1 5000
echo ""

echo "--- GET 'hello' after deletion — should be NOT_FOUND ---"
echo "GET hello" | nc -q1 127.0.0.1 5000
echo ""

echo "=== Demo complete ==="
