#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build hardcode_samples"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod hardcode_samples 2>/dev/null || true
  insmod hardcode_samples.ko
  rmmod hardcode_samples
  echo "[PASS] insmod hardcode_samples"
else
  echo "[SKIP] insmod on $(uname -r); module is built for 5.15.0-173-generic"
fi
