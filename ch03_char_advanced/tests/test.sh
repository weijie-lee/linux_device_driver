#!/bin/bash
set -e
DRIVER="globalfifo"
DEVICE="/dev/globalfifo"
echo "=== Ch03 Globalfifo Character Device Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.3
[ -c "${DEVICE}" ] && echo "  [PASS] ${DEVICE} created" || { echo "[FAIL] ${DEVICE} not found"; exit 1; }
echo "[STEP 3] Running userspace tests..."
gcc -o /tmp/test_globalfifo tests/test_globalfifo.c
/tmp/test_globalfifo
TEST_RET=$?
echo "[STEP 4] Unloading..."
rmmod "${DRIVER}"
rm -f /tmp/test_globalfifo
echo ""
[ $TEST_RET -eq 0 ] && echo "=== Ch03 ALL TESTS PASSED ===" || { echo "=== Ch03 SOME TESTS FAILED ==="; exit 1; }
