#!/bin/bash
# run_all_tests.sh — 一键运行所有章节测试
# 用法：sudo bash run_all_tests.sh
# 注意：需要 root 权限（insmod/rmmod/mount 等操作）

set -e
PASS_CHAPTERS=()
FAIL_CHAPTERS=()

run_chapter_test() {
    local chapter=$1
    local dir=$2
    echo ""
    echo "============================================================"
    echo "  Running: $chapter"
    echo "============================================================"
    if [ ! -d "$dir" ]; then
        echo "  [SKIP] Directory $dir not found"
        return
    fi
    if [ ! -f "$dir/tests/test.sh" ]; then
        echo "  [SKIP] No test.sh in $dir/tests/"
        return
    fi
    pushd "$dir" > /dev/null
    if bash tests/test.sh; then
        PASS_CHAPTERS+=("$chapter")
    else
        FAIL_CHAPTERS+=("$chapter")
    fi
    popd > /dev/null
}

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_DIR"

run_chapter_test "Ch01 Kernel Basics"    "ch01_kernel_basics"
run_chapter_test "Ch02 Char Basic"       "ch02_char_basic"
run_chapter_test "Ch03 Char Advanced"    "ch03_char_advanced"
run_chapter_test "Ch04 Timer"            "ch04_timer"
run_chapter_test "Ch05 Misc"             "ch05_misc"
run_chapter_test "Ch06 Platform"         "ch06_platform"
run_chapter_test "Ch07 Input"            "ch07_input"
run_chapter_test "Ch08 Regmap"           "ch08_regmap"
run_chapter_test "Ch09 Watchdog"         "ch09_watchdog"
run_chapter_test "Ch10 RTC"              "ch10_rtc"
run_chapter_test "Ch11 PWM"              "ch11_pwm"
run_chapter_test "Ch12 DMA"              "ch12_dma"
run_chapter_test "Ch13 Net Virtual"      "ch13_net_virtual"
run_chapter_test "Ch14 Net MAC+PHY"      "ch14_net_mac_phy"
run_chapter_test "Ch15 I2C"              "ch15_i2c"
run_chapter_test "Ch16 SPI"              "ch16_spi"
run_chapter_test "Ch17 Block"            "ch17_block"
run_chapter_test "Ch18 MMC"              "ch18_mmc"

echo ""
echo "============================================================"
echo "  TEST SUMMARY"
echo "============================================================"
echo "  PASSED (${#PASS_CHAPTERS[@]}): ${PASS_CHAPTERS[*]}"
echo "  FAILED (${#FAIL_CHAPTERS[@]}): ${FAIL_CHAPTERS[*]}"
echo ""
[ ${#FAIL_CHAPTERS[@]} -eq 0 ] && echo "  ALL CHAPTERS PASSED!" || echo "  SOME CHAPTERS FAILED!"
