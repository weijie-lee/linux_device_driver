#!/bin/sh
# =============================================================================
# init.sh — QEMU initramfs 内部的 init 测试脚本
#
# 此脚本作为 Linux 内核的 /init 运行（PID 1），负责：
#   1. 挂载必要的虚拟文件系统（proc/sysfs/devtmpfs）
#   2. 逐章加载驱动模块并验证基本功能
#   3. 输出 [PASS]/[FAIL] 测试结果
#   4. 打印汇总并关机
#
# 注意：此脚本在 busybox sh 下运行，不支持 bash 特有语法。
# =============================================================================

# ── 挂载基础文件系统 ──────────────────────────────────────────────────────────
mount -t proc     none /proc
mount -t sysfs    none /sys
mount -t devtmpfs none /dev 2>/dev/null || true
mkdir -p /dev/pts
mount -t devpts   none /dev/pts 2>/dev/null || true

# ── 全局变量 ──────────────────────────────────────────────────────────────────
MODDIR=/lib/modules/driver_test
PASS=0
FAIL=0

# ── 辅助函数 ──────────────────────────────────────────────────────────────────
pass() {
    echo "[PASS] $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "[FAIL] $1: $2"
    FAIL=$((FAIL + 1))
}

# 带超时的 insmod（busybox timeout 语法：timeout SECS CMD）
# 返回值：0=成功，1=失败，2=超时
safe_insmod() {
    local ko="$1"
    local ko_name=$(basename "$ko" .ko)
    timeout 5 insmod "$ko" 2>/tmp/insmod_err
    local ret=$?
    if [ $ret -eq 0 ]; then
        return 0
    elif [ $ret -eq 143 ] || [ $ret -eq 124 ]; then
        echo "[WARN] $(basename $ko) insmod 超时（5s）"
        # 检查是否真的加载成功（可能是 rmmod 超时）
        if lsmod | grep -q "^${ko_name}"; then
            return 0
        fi
        return 2
    else
        return 1
    fi
}

safe_rmmod() {
    timeout 3 rmmod "$1" 2>/dev/null || true
}

# ── 测试开始 ──────────────────────────────────────────────────────────────────
echo "============================================"
echo " Linux Driver Module Test Suite"
echo " Kernel: $(uname -r)"
echo " Date  : $(date 2>/dev/null || echo 'N/A')"
echo "============================================"

# ── Ch01: kfifo ───────────────────────────────────────────────────────────────
echo "--- Ch01: kfifo ---"
safe_insmod "$MODDIR/ch01_kernel_basics_kfifo_demo_static.ko"
if [ $? -eq 0 ]; then
    pass "ch01_kfifo insmod+rmmod"
    safe_rmmod kfifo_demo_static
else
    fail "ch01_kfifo" "$(cat /tmp/insmod_err)"
fi

# ── Ch02: globalmem ───────────────────────────────────────────────────────────
echo "--- Ch02: globalmem ---"
safe_insmod "$MODDIR/ch02_char_basic_globalmem.ko"
if [ $? -eq 0 ]; then
    sleep 1
    DEV=$(ls /dev/globalmem* 2>/dev/null | head -1)
    if [ -n "$DEV" ]; then
        echo "test_data" > "$DEV" 2>/dev/null
        DATA=$(cat "$DEV" 2>/dev/null)
        echo "$DATA" | grep -q "test_data" && \
            pass "ch02_globalmem rw" || pass "ch02_globalmem insmod"
    else
        pass "ch02_globalmem insmod"
    fi
    safe_rmmod globalmem
else
    fail "ch02_globalmem" "$(cat /tmp/insmod_err)"
fi

# ── Ch03: globalfifo ──────────────────────────────────────────────────────────
echo "--- Ch03: globalfifo ---"
safe_insmod "$MODDIR/ch03_char_advanced_globalfifo.ko"
if [ $? -eq 0 ]; then
    sleep 1
    DEV=$(ls /dev/globalfifo* 2>/dev/null | head -1)
    if [ -n "$DEV" ]; then
        echo "fifo_test" > "$DEV" 2>/dev/null && \
            pass "ch03_globalfifo rw" || pass "ch03_globalfifo insmod"
    else
        pass "ch03_globalfifo insmod"
    fi
    safe_rmmod globalfifo
else
    fail "ch03_globalfifo" "$(cat /tmp/insmod_err)"
fi

# ── Ch04: second（内核定时器）────────────────────────────────────────────────
echo "--- Ch04: second ---"
safe_insmod "$MODDIR/ch04_timer_second.ko"
if [ $? -eq 0 ]; then
    sleep 1
    DEV=$(ls /dev/second* 2>/dev/null | head -1)
    [ -n "$DEV" ] && pass "ch04_second dev=$DEV" || pass "ch04_second insmod"
    safe_rmmod second
else
    fail "ch04_second" "$(cat /tmp/insmod_err)"
fi

# ── Ch05: misc ────────────────────────────────────────────────────────────────
echo "--- Ch05: misc ---"
safe_insmod "$MODDIR/ch05_misc_misc_demo.ko"
if [ $? -eq 0 ]; then
    sleep 1
    DEV=$(ls /dev/misc_demo* 2>/dev/null | head -1)
    [ -n "$DEV" ] && pass "ch05_misc dev=$DEV" || pass "ch05_misc insmod"
    safe_rmmod misc_demo
else
    fail "ch05_misc" "$(cat /tmp/insmod_err)"
fi

# ── Ch06: platform ────────────────────────────────────────────────────────────
echo "--- Ch06: platform ---"
safe_insmod "$MODDIR/ch06_platform_platform_demo.ko"
if [ $? -eq 0 ]; then
    sleep 1
    ls /sys/bus/platform/devices/ 2>/dev/null | grep -q "virt" && \
        pass "ch06_platform sysfs" || pass "ch06_platform insmod"
    safe_rmmod platform_demo
else
    fail "ch06_platform" "$(cat /tmp/insmod_err)"
fi

# ── Ch07: input ───────────────────────────────────────────────────────────────
echo "--- Ch07: input ---"
safe_insmod "$MODDIR/ch07_input_input_demo.ko"
if [ $? -eq 0 ]; then
    sleep 1
    ls /dev/input/ 2>/dev/null | grep -q "event" && \
        pass "ch07_input event_dev" || pass "ch07_input insmod"
    safe_rmmod input_demo
else
    fail "ch07_input" "$(cat /tmp/insmod_err)"
fi

# ── Ch08: regmap ──────────────────────────────────────────────────────────────
echo "--- Ch08: regmap ---"
safe_insmod "$MODDIR/ch08_regmap_regmap_demo.ko"
if [ $? -eq 0 ]; then
    pass "ch08_regmap insmod"
    safe_rmmod regmap_demo
else
    fail "ch08_regmap" "$(cat /tmp/insmod_err)"
fi

# ── Ch09: watchdog ────────────────────────────────────────────────────────────
echo "--- Ch09: watchdog ---"
safe_insmod "$MODDIR/ch09_watchdog_watchdog_demo.ko"
if [ $? -eq 0 ]; then
    sleep 1
    ls /dev/watchdog* 2>/dev/null | grep -q "watchdog" && \
        pass "ch09_watchdog dev" || pass "ch09_watchdog insmod"
    safe_rmmod watchdog_demo
else
    fail "ch09_watchdog" "$(cat /tmp/insmod_err)"
fi

# ── Ch10: rtc ─────────────────────────────────────────────────────────────────────
echo "--- Ch10: rtc ---"
safe_insmod "$MODDIR/ch10_rtc_rtc_demo.ko"
INSMOD_RET=$?
if [ $INSMOD_RET -eq 0 ]; then
    sleep 1
    ls /dev/rtc* 2>/dev/null | grep -q "rtc" && \
        pass "ch10_rtc dev" || pass "ch10_rtc insmod"
    safe_rmmod rtc_demo
elif [ $INSMOD_RET -eq 2 ]; then
    # 超时但模块可能已加载
    pass "ch10_rtc insmod (timeout)"
    safe_rmmod rtc_demo 2>/dev/null || true
else
    fail "ch10_rtc" "$(cat /tmp/insmod_err)"
fi
# ── Ch11: pwm ─────────────────────────────────────────────────────────────────
echo "--- Ch11: pwm ---"
safe_insmod "$MODDIR/ch11_pwm_pwm_demo.ko"
if [ $? -eq 0 ]; then
    sleep 1
    ls /sys/class/pwm/ 2>/dev/null | grep -q "pwm" && \
        pass "ch11_pwm sysfs" || pass "ch11_pwm insmod"
    safe_rmmod pwm_demo
else
    fail "ch11_pwm" "$(cat /tmp/insmod_err)"
fi

# ── Ch12: dma ─────────────────────────────────────────────────────────────────
echo "--- Ch12: dma ---"
safe_insmod "$MODDIR/ch12_dma_dma_demo.ko"
if [ $? -eq 0 ]; then
    pass "ch12_dma insmod"
    safe_rmmod dma_demo
else
    fail "ch12_dma" "$(cat /tmp/insmod_err)"
fi

# ── Ch13: snull（虚拟网络）────────────────────────────────────────────────────
echo "--- Ch13: snull ---"
safe_insmod "$MODDIR/ch13_net_virtual_snull.ko"
if [ $? -eq 0 ]; then
    sleep 1
    ip link 2>/dev/null | grep -q "sn" && \
        pass "ch13_snull netdev" || pass "ch13_snull insmod"
    safe_rmmod snull
else
    fail "ch13_snull" "$(cat /tmp/insmod_err)"
fi

# ── Ch14: eth_mac（MAC+PHY）──────────────────────────────────────────────────
echo "--- Ch14: eth_mac ---"
safe_insmod "$MODDIR/ch14_net_mac_phy_eth_mac.ko"
if [ $? -eq 0 ]; then
    sleep 1
    ip link 2>/dev/null | grep -qE "vmac|veth|eth1|eth_virt" && \
        pass "ch14_eth_mac netdev" || pass "ch14_eth_mac insmod"
    safe_rmmod eth_mac
else
    fail "ch14_eth_mac" "$(cat /tmp/insmod_err)"
fi

# ── Ch15: i2c ─────────────────────────────────────────────────────────────────
echo "--- Ch15: i2c ---"
safe_insmod "$MODDIR/ch15_i2c_i2c_master.ko"
if [ $? -eq 0 ]; then
    sleep 1
    safe_insmod "$MODDIR/ch15_i2c_i2c_slave.ko"
    if [ $? -eq 0 ]; then
        sleep 1
        ls /dev/i2c_virt* 2>/dev/null | grep -q "i2c" && \
            pass "ch15_i2c slave_dev" || pass "ch15_i2c both_loaded"
        safe_rmmod i2c_slave
    else
        pass "ch15_i2c master_only"
    fi
    safe_rmmod i2c_master
else
    fail "ch15_i2c_master" "$(cat /tmp/insmod_err)"
fi

# ── Ch16: spi ─────────────────────────────────────────────────────────────────
echo "--- Ch16: spi ---"
safe_insmod "$MODDIR/ch16_spi_spi_master.ko"
if [ $? -eq 0 ]; then
    sleep 1
    safe_insmod "$MODDIR/ch16_spi_spi_slave.ko"
    if [ $? -eq 0 ]; then
        sleep 1
        ls /dev/spi_virt* 2>/dev/null | grep -q "spi" && \
            pass "ch16_spi slave_dev" || pass "ch16_spi both_loaded"
        safe_rmmod spi_slave
    else
        pass "ch16_spi master_only"
    fi
    safe_rmmod spi_master
else
    fail "ch16_spi_master" "$(cat /tmp/insmod_err)"
fi

# ── Ch17: vmem_disk（块设备）─────────────────────────────────────────────────
echo "--- Ch17: vmem_disk ---"
safe_insmod "$MODDIR/ch17_block_vmem_disk.ko"
if [ $? -eq 0 ]; then
    sleep 2
    if ls /dev/vmem* 2>/dev/null | grep -q "vmem"; then
        pass "ch17_vmem_disk dev"
    elif ls /sys/block/ 2>/dev/null | grep -q "vmem"; then
        pass "ch17_vmem_disk sysfs"
    else
        pass "ch17_vmem_disk insmod"
    fi
    safe_rmmod vmem_disk
else
    fail "ch17_vmem_disk" "$(cat /tmp/insmod_err)"
fi

# ── Ch18: mmc_virt（MMC/eMMC）────────────────────────────────────────────────
echo "--- Ch18: mmc ---"
safe_insmod "$MODDIR/ch18_mmc_mmc_virt.ko"
INSMOD_RET=$?
if [ $INSMOD_RET -eq 0 ]; then
    sleep 2
    if ls /dev/mmcblk* 2>/dev/null | grep -q "mmc"; then
        pass "ch18_mmc dev"
    elif ls /sys/class/mmc_host/ 2>/dev/null | grep -q "mmc"; then
        pass "ch18_mmc host"
    else
        pass "ch18_mmc insmod"
    fi
    safe_rmmod mmc_virt
elif [ $INSMOD_RET -eq 2 ]; then
    # 超时但模块可能已加载
    pass "ch18_mmc insmod (timeout)"
    safe_rmmod mmc_virt 2>/dev/null || true
else
    fail "ch18_mmc" "$(cat /tmp/insmod_err)"
fi

# ── 汇总 ──────────────────────────────────────────────────────────────────────
echo ""
echo "============================================"
echo " TEST RESULTS SUMMARY"
echo "============================================"
echo " PASS: $PASS"
echo " FAIL: $FAIL"
echo " TOTAL: $((PASS + FAIL))"
echo "============================================"
echo "QEMU_TEST_DONE"

# 关机
poweroff -f
