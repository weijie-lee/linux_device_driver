#!/bin/bash
# Ch06 Platform 设备驱动自动化测试脚本
set -e
DRIVER="platform_demo"
DEVICE="/dev/virt_plat"

echo "=== Ch06 Platform Device Driver Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true

echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"

echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.5
[ -c "${DEVICE}" ] && echo "  [PASS] ${DEVICE} created" || { echo "[FAIL] ${DEVICE} not found"; exit 1; }

echo "[STEP 3] Check sysfs..."
ls /sys/bus/platform/devices/ | grep -q "virt_plat" && \
    echo "  [PASS] Found in /sys/bus/platform/devices/" || \
    echo "  [WARN] Not found in sysfs"

echo "[STEP 4] Running userspace tests..."
gcc -o /tmp/test_platform tests/test_platform.c
/tmp/test_platform
TEST_RET=$?

echo "[STEP 5] Unloading..."
rmmod "${DRIVER}"
rm -f /tmp/test_platform

echo ""
[ $TEST_RET -eq 0 ] && echo "=== Ch06 ALL TESTS PASSED ===" || { echo "=== Ch06 SOME TESTS FAILED ==="; exit 1; }
