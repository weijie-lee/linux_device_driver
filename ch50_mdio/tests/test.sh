#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_mdio"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_mdio 2>/dev/null || true
  insmod virt_mdio.ko
  rmmod virt_mdio
  echo "[PASS] insmod virt_mdio"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
