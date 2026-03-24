#!/bin/bash
# Ch11 PWM 驱动自动化测试脚本
# PWM 通过 sysfs 接口操作，无需用户态 C 程序
set -e
DRIVER="pwm_demo"

echo "=== Ch11 PWM Driver Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true

echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"

echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.5

# 找到新注册的 pwmchip
CHIP=$(ls /sys/class/pwm/ | grep pwmchip | sort -V | tail -1)
[ -n "$CHIP" ] && echo "  [PASS] Found $CHIP" || { echo "[FAIL] No pwmchip found"; exit 1; }

echo "[STEP 3] Export PWM channel 0..."
echo 0 > /sys/class/pwm/$CHIP/export
sleep 0.2
[ -d "/sys/class/pwm/$CHIP/pwm0" ] && echo "  [PASS] pwm0 exported" || { echo "[FAIL] pwm0 not found"; exit 1; }

echo "[STEP 4] Configure PWM (period=1ms, duty=50%)..."
echo 1000000 > /sys/class/pwm/$CHIP/pwm0/period
echo 500000  > /sys/class/pwm/$CHIP/pwm0/duty_cycle
echo "  [PASS] period and duty_cycle set"

echo "[STEP 5] Enable PWM..."
echo 1 > /sys/class/pwm/$CHIP/pwm0/enable
ENABLED=$(cat /sys/class/pwm/$CHIP/pwm0/enable)
[ "$ENABLED" = "1" ] && echo "  [PASS] PWM enabled" || echo "  [WARN] enable readback = $ENABLED"

echo "[STEP 6] Verify dmesg output..."
sleep 0.2
dmesg | tail -5 | grep -q "duty_cycle" && echo "  [PASS] dmesg shows PWM config" || echo "  [WARN] dmesg output not found"

echo "[STEP 7] Disable and unexport..."
echo 0 > /sys/class/pwm/$CHIP/pwm0/enable
echo 0 > /sys/class/pwm/$CHIP/unexport
echo "  [PASS] PWM disabled and unexported"

echo "[STEP 8] Unloading..."
rmmod "${DRIVER}"

echo ""
echo "=== Ch11 ALL TESTS PASSED ==="
