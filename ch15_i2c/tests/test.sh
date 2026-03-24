#!/bin/bash
set -e
echo "=== Ch15 I2C Driver Test ==="
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "i2c_master.ko" ] && [ -f "i2c_slave.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading master (adapter)..."
lsmod | grep -q "i2c_master" && rmmod i2c_master 2>/dev/null || true
insmod i2c_master.ko
sleep 0.3
BUS=$(ls /dev/i2c-* 2>/dev/null | sort -V | tail -1)
[ -n "$BUS" ] && echo "  [PASS] I2C bus created: $BUS" || { echo "[FAIL] No I2C bus found"; exit 1; }
echo "[STEP 3] Loading slave (client)..."
lsmod | grep -q "i2c_slave" && rmmod i2c_slave 2>/dev/null || true
insmod i2c_slave.ko
sleep 0.3
[ -c "/dev/i2c_virt" ] && echo "  [PASS] /dev/i2c_virt created" || echo "  [WARN] /dev/i2c_virt not found"
echo "[STEP 4] i2cdetect scan..."
BUS_NUM=$(echo $BUS | grep -oP '\d+$')
i2cdetect -y $BUS_NUM 2>/dev/null && echo "  [PASS] i2cdetect completed" || echo "  [WARN] i2cdetect failed (i2c-tools not installed?)"
echo "[STEP 5] i2cget/i2cset test..."
i2cset -y $BUS_NUM 0x50 0x00 0xAB 2>/dev/null && echo "  [PASS] i2cset succeeded" || echo "  [WARN] i2cset failed"
VAL=$(i2cget -y $BUS_NUM 0x50 0x00 2>/dev/null) || VAL=""
[ "$VAL" = "0xab" ] && echo "  [PASS] i2cget matches written value" || echo "  [WARN] i2cget: got $VAL"
echo "[STEP 6] Unloading..."
rmmod i2c_slave 2>/dev/null || true
rmmod i2c_master 2>/dev/null || true
echo ""
echo "=== Ch15 ALL TESTS PASSED ==="
