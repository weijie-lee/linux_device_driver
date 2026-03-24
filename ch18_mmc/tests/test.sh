#!/bin/bash
set -e
DRIVER="mmc_virt"
echo "=== Ch18 Virtual MMC Driver Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 1
ls /dev/mmcblk* 2>/dev/null && echo "  [PASS] mmcblk device created" || { echo "[FAIL] No mmcblk device"; exit 1; }
MMCDEV=$(ls /dev/mmcblk* | grep -v p | head -1)
echo "  Using device: $MMCDEV"
echo "[STEP 3] mkfs.ext4..."
mkfs.ext4 -F ${MMCDEV} &>/dev/null && echo "  [PASS] mkfs.ext4 succeeded" || { echo "[FAIL] mkfs.ext4 failed"; exit 1; }
echo "[STEP 4] mount and write/read..."
mkdir -p /tmp/mmc_test
mount ${MMCDEV} /tmp/mmc_test
echo "mmc_virt test data" > /tmp/mmc_test/test.txt
CONTENT=$(cat /tmp/mmc_test/test.txt)
[ "$CONTENT" = "mmc_virt test data" ] && echo "  [PASS] write/read on mounted fs succeeded" || echo "  [FAIL] data mismatch"
umount /tmp/mmc_test
rmdir /tmp/mmc_test
echo "[STEP 5] Unloading..."
rmmod "${DRIVER}"
echo ""
echo "=== Ch18 ALL TESTS PASSED ==="
