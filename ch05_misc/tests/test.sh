#!/bin/bash
# Ch05 Misc 设备驱动自动化测试脚本
# 用法：sudo bash tests/test.sh

set -e
DRIVER="misc_demo"
DEVICE="/dev/misc_demo"
KO="../ch05_misc/${DRIVER}.ko"
# 支持从章节目录直接运行
[ -f "${DRIVER}.ko" ] && KO="${DRIVER}.ko"

echo "=== Ch05 Misc Device Driver Test ==="
echo ""

# 清理旧模块
lsmod | grep -q "${DRIVER}" && rmmod "${DRIVER}" 2>/dev/null || true

# 编译
echo "[STEP 1] Building driver..."
make -C .. ch05_misc 2>/dev/null || make 2>/dev/null
[ -f "${DRIVER}.ko" ] || { echo "[FAIL] Build failed"; exit 1; }
echo "  [PASS] Build succeeded"

# 加载模块
echo "[STEP 2] Loading module..."
insmod "${DRIVER}.ko"
sleep 0.3
[ -c "${DEVICE}" ] && echo "  [PASS] ${DEVICE} created" || { echo "  [FAIL] ${DEVICE} not found"; exit 1; }

# 检查 /proc/misc
echo "[STEP 3] Check /proc/misc..."
grep -q "${DRIVER}" /proc/misc && echo "  [PASS] Found in /proc/misc" || echo "  [WARN] Not found in /proc/misc"

# 编译并运行用户态测试
echo "[STEP 4] Running userspace tests..."
gcc -o /tmp/test_misc tests/test_misc.c 2>/dev/null || \
    gcc -o /tmp/test_misc test_misc.c
/tmp/test_misc
TEST_RET=$?

# 卸载模块
echo ""
echo "[STEP 5] Unloading module..."
rmmod "${DRIVER}"
[ ! -c "${DEVICE}" ] && echo "  [PASS] ${DEVICE} removed" || echo "  [WARN] ${DEVICE} still exists"

# 清理临时文件
rm -f /tmp/test_misc

echo ""
if [ $TEST_RET -eq 0 ]; then
    echo "=== Ch05 ALL TESTS PASSED ==="
else
    echo "=== Ch05 SOME TESTS FAILED ==="
    exit 1
fi
