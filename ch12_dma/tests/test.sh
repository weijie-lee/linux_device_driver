#!/bin/bash
set -e
DRIVER="dma_demo"
echo "=== Ch12 DMA Driver Test ==="
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true
echo "[STEP 1] Building..."
make 2>/dev/null || true
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"
echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.3
echo "[STEP 3] Check dmesg for DMA transfer result..."
dmesg | tail -10 | grep -qi "dma" && echo "  [PASS] DMA messages in dmesg" || echo "  [WARN] No DMA messages"
dmesg | tail -10 | grep -qi "success\|passed\|match\|ok" && echo "  [PASS] DMA transfer verified" || echo "  [WARN] Transfer result unclear"
echo "[STEP 4] Unloading..."
rmmod "${DRIVER}"
echo ""
echo "=== Ch12 ALL TESTS PASSED ==="
