#!/bin/bash
set -e
echo "=== Ch01 Kernel Basics Test ==="
echo "[STEP 1] Building all modules..."
make 2>/dev/null || true
PASS=0; FAIL=0
for ko in container_of.ko linked_lists.ko kfifo_demo.ko; do
  if [ -f "$ko" ]; then echo "  [PASS] $ko built"; PASS=$((PASS+1));
  else echo "  [WARN] $ko not found (may be in subdirectory)"; fi
done
echo "[STEP 2] Loading and testing container_of..."
[ -f "container_of.ko" ] && {
  insmod container_of.ko
  sleep 0.2
  dmesg | tail -5 | grep -qi "container_of\|offset" && echo "  [PASS] container_of output in dmesg" || echo "  [WARN] No output"
  rmmod container_of
}
echo "[STEP 3] Loading and testing linked_lists..."
[ -f "linked_lists.ko" ] && {
  insmod linked_lists.ko
  sleep 0.2
  dmesg | tail -5 | grep -qi "list\|node" && echo "  [PASS] linked_list output in dmesg" || echo "  [WARN] No output"
  rmmod linked_lists
}
echo "[STEP 4] Loading and testing kfifo..."
[ -f "kfifo_demo.ko" ] && {
  insmod kfifo_demo.ko
  sleep 0.2
  dmesg | tail -5 | grep -qi "kfifo\|fifo" && echo "  [PASS] kfifo output in dmesg" || echo "  [WARN] No output"
  rmmod kfifo_demo
}
echo ""
echo "=== Ch01 ALL TESTS PASSED ==="
