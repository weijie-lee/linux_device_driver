#!/bin/bash
set -e
DRIVER="snull"
echo "=== Ch13 snull Virtual Network Device Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.5
ip link show sn0 &>/dev/null && echo "  [PASS] sn0 interface created" || { echo "[FAIL] sn0 not found"; exit 1; }
ip link show sn1 &>/dev/null && echo "  [PASS] sn1 interface created" || { echo "[FAIL] sn1 not found"; exit 1; }
echo "[STEP 3] Bring up interfaces..."
ip link set sn0 up
ip link set sn1 up
ip addr add 192.168.100.1/24 dev sn0
ip addr add 192.168.101.1/24 dev sn1
echo "  [PASS] Interfaces configured"
echo "[STEP 4] Check interface stats..."
ip -s link show sn0 | grep -q "RX:" && echo "  [PASS] sn0 stats available" || echo "  [WARN] stats not found"
echo "[STEP 5] Unloading..."
ip link set sn0 down 2>/dev/null || true
ip link set sn1 down 2>/dev/null || true
rmmod "${DRIVER}"
echo ""
echo "=== Ch13 ALL TESTS PASSED ==="
