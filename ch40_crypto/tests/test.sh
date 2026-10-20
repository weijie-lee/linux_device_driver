#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_crypto"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_crypto 2>/dev/null || true
  insmod virt_crypto.ko
  rmmod virt_crypto
  echo "[PASS] insmod virt_crypto"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
