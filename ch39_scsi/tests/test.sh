#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_scsi"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_scsi 2>/dev/null || true
  insmod virt_scsi.ko
  rmmod virt_scsi
  echo "[PASS] insmod virt_scsi"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
