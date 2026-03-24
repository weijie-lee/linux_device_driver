#!/bin/bash
set -e
DRIVER="rtc_demo"
echo "=== Ch10 RTC Driver Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.5
ls /dev/rtc* 2>/dev/null && echo "  [PASS] RTC device found" || { echo "[FAIL] No RTC device"; exit 1; }
echo "[STEP 3] Running userspace tests..."
gcc -o /tmp/test_rtc tests/test_rtc.c
/tmp/test_rtc
TEST_RET=$?
echo "[STEP 4] Unloading..."
rmmod "${DRIVER}"
rm -f /tmp/test_rtc
echo ""
[ $TEST_RET -eq 0 ] && echo "=== Ch10 ALL TESTS PASSED ===" || { echo "=== Ch10 SOME TESTS FAILED ==="; exit 1; }
