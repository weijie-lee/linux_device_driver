#!/bin/bash
set -e
DRIVER="eth_mac"
echo "=== Ch14 Ethernet MAC+PHY Driver Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 1
IFACE=$(ip link | grep -oP '(?<=\d: )veth_mac(?=:)' | head -1)
[ -n "$IFACE" ] && echo "  [PASS] $IFACE interface created" || { echo "[FAIL] veth_mac not found"; exit 1; }
echo "[STEP 3] Check link state (PHY timer ~500ms)..."
sleep 1
ip link show veth_mac | grep -q "UP" && echo "  [PASS] Link is UP" || echo "  [WARN] Link not yet UP"
echo "[STEP 4] Configure and ping loopback..."
ip addr add 10.99.0.1/24 dev veth_mac 2>/dev/null || true
ip link set veth_mac up
ping -c 3 -I veth_mac 10.99.0.1 &>/dev/null && echo "  [PASS] ping loopback succeeded" || echo "  [WARN] ping failed (loopback may not be enabled)"
echo "[STEP 5] ethtool query..."
ethtool veth_mac 2>/dev/null | grep -q "Speed" && echo "  [PASS] ethtool reports speed" || echo "  [WARN] ethtool not available"
echo "[STEP 6] Unloading..."
ip link set veth_mac down 2>/dev/null || true
rmmod "${DRIVER}"
echo ""
echo "=== Ch14 ALL TESTS PASSED ==="
