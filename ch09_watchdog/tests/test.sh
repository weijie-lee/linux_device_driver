#!/bin/bash
set -e
DRIVER="watchdog_demo"
echo "=== Ch09 Watchdog Driver Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading module (timeout=10)..."
insmod "${DRIVER}.ko" timeout=10
sleep 0.3
ls /dev/watchdog* 2>/dev/null && echo "  [PASS] /dev/watchdog* created" || { echo "[FAIL] watchdog device not found"; exit 1; }
echo "[STEP 3] Running userspace tests..."
gcc -o /tmp/test_watchdog tests/test_watchdog.c
/tmp/test_watchdog
TEST_RET=$?
echo "[STEP 4] Unloading..."
rmmod "${DRIVER}" 2>/dev/null || true
rm -f /tmp/test_watchdog
echo ""
[ $TEST_RET -eq 0 ] && echo "=== Ch09 ALL TESTS PASSED ===" || { echo "=== Ch09 SOME TESTS FAILED ==="; exit 1; }
