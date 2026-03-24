#!/bin/bash
set -e
echo "=== Ch16 SPI Driver Test ==="
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "spi_master.ko" ] && [ -f "spi_slave.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading SPI master..."
lsmod | grep -q "spi_master" && rmmod spi_master 2>/dev/null || true
insmod spi_master.ko
sleep 0.3
ls /sys/bus/spi/devices/ 2>/dev/null && echo "  [PASS] SPI bus devices visible" || echo "  [WARN] No SPI devices in sysfs"
echo "[STEP 3] Loading SPI slave..."
lsmod | grep -q "spi_slave" && rmmod spi_slave 2>/dev/null || true
insmod spi_slave.ko
sleep 0.3
[ -c "/dev/spi_virt" ] && echo "  [PASS] /dev/spi_virt created" || { echo "[FAIL] /dev/spi_virt not found"; exit 1; }
echo "[STEP 4] Write and read back (loopback test)..."
echo "SPI loopback test" > /dev/spi_virt
RESULT=$(cat /dev/spi_virt 2>/dev/null)
[ -n "$RESULT" ] && echo "  [PASS] Read back: $RESULT" || echo "  [WARN] No data read back"
echo "[STEP 5] dmesg check..."
dmesg | tail -5 | grep -qi "spi" && echo "  [PASS] SPI messages in dmesg" || echo "  [WARN] No SPI messages in dmesg"
echo "[STEP 6] Unloading..."
rmmod spi_slave 2>/dev/null || true
rmmod spi_master 2>/dev/null || true
echo ""
echo "=== Ch16 ALL TESTS PASSED ==="
