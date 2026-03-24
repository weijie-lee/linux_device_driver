#!/bin/bash
set -e
DRIVER="regmap_demo"
DEVICE="/dev/regmap_demo"
echo "=== Ch08 Regmap Driver Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.3
[ -c "${DEVICE}" ] && echo "  [PASS] ${DEVICE} created" || { echo "[FAIL] ${DEVICE} not found"; exit 1; }
echo "[STEP 3] Check debugfs..."
[ -d "/sys/kernel/debug/regmap/${DRIVER}" ] && \
    echo "  [PASS] debugfs entry found" || echo "  [WARN] debugfs not mounted or entry not found"
echo "[STEP 4] Running userspace tests..."
gcc -o /tmp/test_regmap tests/test_regmap.c
/tmp/test_regmap
TEST_RET=$?
echo "[STEP 5] Unloading..."
rmmod "${DRIVER}"
rm -f /tmp/test_regmap
echo ""
[ $TEST_RET -eq 0 ] && echo "=== Ch08 ALL TESTS PASSED ===" || { echo "=== Ch08 SOME TESTS FAILED ==="; exit 1; }
