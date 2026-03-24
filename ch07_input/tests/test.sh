#!/bin/bash
# Ch07 Input 子系统驱动自动化测试脚本
set -e
DRIVER="input_demo"

echo "=== Ch07 Input Subsystem Driver Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true

echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"

echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.5
[ -f "/proc/virt_kbd" ] && echo "  [PASS] /proc/virt_kbd created" || { echo "[FAIL] /proc/virt_kbd not found"; exit 1; }

echo "[STEP 3] Check input device..."
grep -r "Virtual Keyboard" /sys/class/input/*/device/name 2>/dev/null && \
    echo "  [PASS] Virtual Keyboard found in sysfs" || echo "  [WARN] Not found in sysfs"

echo "[STEP 4] Running userspace tests..."
gcc -o /tmp/test_input tests/test_input.c
/tmp/test_input
TEST_RET=$?

echo "[STEP 5] Unloading..."
rmmod "${DRIVER}"
rm -f /tmp/test_input

echo ""
[ $TEST_RET -eq 0 ] && echo "=== Ch07 ALL TESTS PASSED ===" || { echo "=== Ch07 SOME TESTS FAILED ==="; exit 1; }
