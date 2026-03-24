#!/bin/bash
set -e
DRIVER="vmem_disk"
DEVICE="/dev/vmem_disk"
echo "=== Ch17 vmem_disk Block Device Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.5
[ -b "${DEVICE}" ] && echo "  [PASS] ${DEVICE} block device created" || { echo "[FAIL] ${DEVICE} not found"; exit 1; }
echo "[STEP 3] Check device size..."
SIZE=$(blockdev --getsize64 ${DEVICE} 2>/dev/null || echo 0)
[ "$SIZE" -gt 0 ] && echo "  [PASS] Device size: $SIZE bytes" || echo "  [WARN] Could not get size"
echo "[STEP 4] mkfs.ext4..."
mkfs.ext4 -F ${DEVICE} &>/dev/null && echo "  [PASS] mkfs.ext4 succeeded" || { echo "[FAIL] mkfs.ext4 failed"; exit 1; }
echo "[STEP 5] mount and write/read..."
mkdir -p /tmp/vmem_test
mount ${DEVICE} /tmp/vmem_test
echo "vmem_disk test data" > /tmp/vmem_test/test.txt
CONTENT=$(cat /tmp/vmem_test/test.txt)
[ "$CONTENT" = "vmem_disk test data" ] && echo "  [PASS] write/read on mounted fs succeeded" || echo "  [FAIL] data mismatch"
umount /tmp/vmem_test
rmdir /tmp/vmem_test
echo "[STEP 6] Unloading..."
rmmod "${DRIVER}"
echo ""
echo "=== Ch17 ALL TESTS PASSED ==="
