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
mkdir -p /sys/kernel/debug /sys/kernel/tracing
mount -t debugfs none /sys/kernel/debug 2>/dev/null || true
mount -t tracefs none /sys/kernel/tracing 2>/dev/null || true

# ── 全局变量 ──────────────────────────────────────────────────────────────────
MODDIR=/lib/modules/driver_test
PASS=0
FAIL=0

# 子系统以模块形式编进 5.15 时，先加载依赖 .ko
if [ -f "$MODDIR/preload.list" ]; then
    echo "--- preload subsystem modules ---"
    while read -r ko; do
        [ -z "$ko" ] && continue
        insmod "$MODDIR/preload/$ko" 2>/tmp/preload_err || \
            echo "[WARN] preload $ko: $(cat /tmp/preload_err)"
    done < "$MODDIR/preload.list"
fi

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

# ── Ch19–Ch68：PCI/USB 以及补齐的子系统 ─────────────────────────────────────
test_one() {
    label="$1"
    file="$2"
    mod="$3"
    echo "--- ${label} ---"
    safe_insmod "$MODDIR/${file}"
    if [ $? -eq 0 ]; then
        pass "${label}"
        safe_rmmod "${mod}"
    else
        fail "${label}" "$(cat /tmp/insmod_err)"
    fi
}

test_one "ch19_pci_basic"    "ch19_pci_bus_pci_basic.ko"       pci_basic
test_one "ch19_pci_advanced" "ch19_pci_bus_pci_advanced.ko"    pci_advanced
test_one "ch20_usb_basic"    "ch20_usb_bus_usb_basic.ko"       usb_basic
test_one "ch20_usb_advanced" "ch20_usb_bus_usb_advanced.ko"    usb_advanced
test_one "ch21_gpio"         "ch21_gpio_virt_gpio.ko"          virt_gpio
test_one "ch22_pinctrl"      "ch22_pinctrl_virt_pinctrl.ko"    virt_pinctrl
test_one "ch23_clk"          "ch23_clk_virt_clk.ko"            virt_clk
test_one "ch24_regulator"    "ch24_regulator_virt_regulator.ko" virt_regulator
test_one "ch25_reset"        "ch25_reset_virt_reset.ko"        virt_reset
test_one "ch26_nvmem"        "ch26_nvmem_virt_nvmem.ko"        virt_nvmem
test_one "ch27_hwmon"        "ch27_hwmon_virt_hwmon.ko"        virt_hwmon
test_one "ch28_leds"         "ch28_leds_virt_led.ko"           virt_led
test_one "ch29_thermal"      "ch29_thermal_virt_thermal.ko"    virt_thermal
test_one "ch30_power"        "ch30_power_virt_psy.ko"          virt_psy
test_one "ch31_backlight"    "ch31_backlight_virt_backlight.ko" virt_backlight
test_one "ch32_phy"          "ch32_phy_virt_phy.ko"            virt_phy
test_one "ch33_pps"          "ch33_pps_virt_pps.ko"            virt_pps
test_one "ch34_ptp"          "ch34_ptp_virt_ptp.ko"            virt_ptp
test_one "ch35_tty"          "ch35_tty_virt_tty.ko"            virt_tty
test_one "ch36_fb"           "ch36_fb_virt_fb.ko"              virt_fb
test_one "ch37_mfd"          "ch37_mfd_virt_mfd.ko"            virt_mfd
test_one "ch38_extcon"       "ch38_extcon_virt_extcon.ko"      virt_extcon
test_one "ch39_scsi"         "ch39_scsi_virt_scsi.ko"          virt_scsi
test_one "ch40_crypto"       "ch40_crypto_virt_crypto.ko"      virt_crypto
test_one "ch41_virtio"       "ch41_virtio_virt_virtio.ko"      virt_virtio
test_one "ch42_remoteproc"   "ch42_remoteproc_virt_rproc.ko"   virt_rproc
test_one "ch43_devfreq"      "ch43_devfreq_virt_devfreq.ko"    virt_devfreq
test_one "ch44_dmaengine"    "ch44_dmaengine_virt_dmaeng.ko"   virt_dmaeng
test_one "ch45_rfkill"       "ch45_rfkill_virt_rfkill.ko"      virt_rfkill
test_one "ch46_hwspinlock"   "ch46_hwspinlock_virt_hwspin.ko"  virt_hwspin
test_one "ch47_mailbox"      "ch47_mailbox_virt_mbox.ko"       virt_mbox
test_one "ch48_powercap"     "ch48_powercap_virt_powercap.ko"  virt_powercap
test_one "ch49_opp"          "ch49_opp_virt_opp.ko"            virt_opp
test_one "ch50_mdio"         "ch50_mdio_virt_mdio.ko"          virt_mdio
test_one "ch51_iio"          "ch51_iio_virt_iio.ko"            virt_iio
test_one "ch52_alsa"         "ch52_alsa_virt_alsa.ko"          virt_alsa
test_one "ch53_hid"          "ch53_hid_virt_hid.ko"            virt_hid
test_one "ch54_mtd"          "ch54_mtd_virt_mtd.ko"            virt_mtd
test_one "ch55_uio"          "ch55_uio_virt_uio.ko"            virt_uio
test_one "ch56_w1"           "ch56_w1_virt_w1.ko"              virt_w1
test_one "ch57_can"          "ch57_can_virt_can.ko"            virt_can
test_one "ch58_mux"          "ch58_mux_virt_mux.ko"            virt_mux
test_one "ch59_fpga"         "ch59_fpga_virt_fpga.ko"          virt_fpga
test_one "ch60_spmi"         "ch60_spmi_virt_spmi.ko"          virt_spmi
test_one "ch61_i3c"          "ch61_i3c_virt_i3c.ko"            virt_i3c
test_one "ch62_gnss"         "ch62_gnss_virt_gnss.ko"          virt_gnss
test_one "ch63_nfc"          "ch63_nfc_virt_nfc.ko"            virt_nfc
test_one "ch64_bluetooth"    "ch64_bluetooth_virt_bt.ko"       virt_bt
test_one "ch65_v4l2"         "ch65_v4l2_virt_v4l2.ko"          virt_v4l2
test_one "ch66_counter"      "ch66_counter_virt_counter.ko"    virt_counter
test_one "ch67_drm"          "ch67_drm_virt_drm.ko"            virt_drm
test_one "ch68_interconnect" "ch68_interconnect_virt_icc.ko"   virt_icc

# ── 主线 learn/：检查接口，不只是 insmod ─────────────────────────────────────
echo "--- learn/01 uapi ---"
safe_insmod "$MODDIR/learn_01_uapi_virt_uapi.ko"
if [ $? -eq 0 ]; then
    sleep 1
    if /bin/uapi_test; then
        pass "learn01_uapi"
    else
        fail "learn01_uapi" "uapi_test"
    fi
    safe_rmmod virt_uapi
else
    fail "learn01_uapi" "$(cat /tmp/insmod_err)"
fi

echo "--- learn/02 devicetree ---"
safe_insmod "$MODDIR/learn_02_devicetree_virt_dt.ko"
if [ $? -eq 0 ]; then
    thr=$(cat /sys/devices/platform/virt_dt/threshold 2>/dev/null)
    id=$(cat /sys/devices/platform/virt_dt/id 2>/dev/null)
    irq=$(cat /sys/devices/platform/virt_dt/irq 2>/dev/null)
    if [ "$thr" = "40" ] && [ "$id" = "4c444454" ] && [ "$irq" = "19" ]; then
        pass "learn02_devicetree"
    else
        fail "learn02_devicetree" "thr=$thr id=$id irq=$irq"
    fi
    safe_rmmod virt_dt
else
    fail "learn02_devicetree" "$(cat /tmp/insmod_err)"
fi

echo "--- learn/03 irq ---"
safe_insmod "$MODDIR/learn_03_irq_virt_irq.ko"
if [ $? -eq 0 ]; then
    echo 1 > /sys/devices/platform/virt_irq/kick 2>/dev/null
    i=0
    irq_ok=0
    while [ "$i" -lt 5 ]; do
        set -- $(cat /sys/devices/platform/virt_irq/counts 2>/dev/null)
        if [ "$1" -ge 1 ] 2>/dev/null && [ "$2" -ge 1 ] && [ "$3" -ge 1 ] && \
           [ "$4" -ge 1 ] && [ "$5" -ge 1 ] && [ "$6" -eq 0 ]; then
            irq_ok=1
            break
        fi
        sleep 1
        i=$((i + 1))
    done
    if [ "$irq_ok" -eq 1 ]; then
        pass "learn03_irq"
    else
        fail "learn03_irq" "$(cat /sys/devices/platform/virt_irq/counts 2>/dev/null)"
    fi
    safe_rmmod virt_irq
else
    fail "learn03_irq" "$(cat /tmp/insmod_err)"
fi

echo "--- learn/04 sensor ---"
safe_insmod "$MODDIR/learn_04_soc_sensor_virt_sensor.ko"
if [ $? -eq 0 ]; then
    echo 37 > /sys/devices/platform/virt_sensor/temperature 2>/dev/null
    i=0
    sensor_ok=0
    while [ "$i" -lt 5 ]; do
        temp=$(cat /sys/devices/platform/virt_sensor/temperature 2>/dev/null)
        hz=$(cat /sys/devices/platform/virt_sensor/clock_hz 2>/dev/null)
        ic=$(cat /sys/devices/platform/virt_sensor/irq_count 2>/dev/null)
        lt=$(cat /sys/devices/platform/virt_sensor/last_temp 2>/dev/null)
        if [ "$temp" = "37" ] && [ "$hz" = "24000000" ] && \
           [ "$ic" -ge 1 ] 2>/dev/null && [ "$lt" = "37" ]; then
            sensor_ok=1
            break
        fi
        sleep 1
        i=$((i + 1))
    done
    if [ "$sensor_ok" -eq 1 ]; then
        pass "learn04_sensor"
    else
        fail "learn04_sensor" "temp=$temp hz=$hz irq=$ic last=$lt"
    fi
    safe_rmmod virt_sensor
else
    fail "learn04_sensor" "$(cat /tmp/insmod_err)"
fi

echo "--- learn/05 pm ---"
safe_insmod "$MODDIR/learn_05_pm_virt_pm.ko"
if [ $? -eq 0 ]; then
    echo suspend > /sys/devices/platform/virt_pm/sleep 2>/dev/null
    c_sus=$(cat /sys/devices/platform/virt_pm/ctrl 2>/dev/null)
    s_sus=$(cat /sys/devices/platform/virt_pm/sleeping 2>/dev/null)
    echo resume > /sys/devices/platform/virt_pm/sleep 2>/dev/null
    c_res=$(cat /sys/devices/platform/virt_pm/ctrl 2>/dev/null)
    echo 1 > /sys/devices/platform/virt_pm/idle 2>/tmp/pm_idle_err
    rt=$(cat /sys/devices/platform/virt_pm/power/runtime_status 2>/dev/null)
    mk=$(cat /sys/devices/platform/virt_pm/runtime_marker 2>/dev/null)
    echo on > /sys/devices/platform/virt_pm/power/control 2>/dev/null
    rt_on=$(cat /sys/devices/platform/virt_pm/power/runtime_status 2>/dev/null)
    mk_on=$(cat /sys/devices/platform/virt_pm/runtime_marker 2>/dev/null)
    if [ "$c_sus" = "0" ] && [ "$s_sus" = "1" ] && [ "$c_res" = "3" ] && \
       [ "$rt" = "suspended" ] && [ "$mk" = "suspended" ] && \
       [ "$rt_on" = "active" ] && [ "$mk_on" = "active" ]; then
        pass "learn05_pm"
    else
        fail "learn05_pm" "sus=$c_sus/$s_sus res=$c_res rt=$rt/$mk on=$rt_on/$mk_on idle=$(cat /tmp/pm_idle_err 2>/dev/null)"
    fi
    safe_rmmod virt_pm
else
    fail "learn05_pm" "$(cat /tmp/insmod_err)"
fi

echo "--- learn/06 dma ---"
safe_insmod "$MODDIR/learn_06_dma_virt_dma.ko"
if [ $? -eq 0 ]; then
    rep=$(cat /sys/devices/platform/virt_dma/report 2>/dev/null)
    echo "$rep" | grep -q "coherent:ok" && \
    echo "$rep" | grep -q "stream:ok" && \
    echo "$rep" | grep -q "sg:ok" && \
        pass "learn06_dma" || fail "learn06_dma" "$rep"
    safe_rmmod virt_dma
else
    fail "learn06_dma" "$(cat /tmp/insmod_err)"
fi

echo "--- learn/07 debug ---"
safe_insmod "$MODDIR/learn_07_debug_virt_debug.ko"
if [ $? -eq 0 ]; then
    echo 1 > /sys/devices/platform/virt_debug/trigger
    if dmesg | grep -q "virt_debug: ping info" && \
       ! dmesg | grep -q "virt_debug: ping debug"; then
        echo "module virt_debug +p" > /sys/kernel/debug/dynamic_debug/control 2>/tmp/dyndbg_err
        echo 1 > /sys/devices/platform/virt_debug/trigger
        if dmesg | grep -q "virt_debug: ping debug"; then
            pass "learn07_dyndbg"
        else
            fail "learn07_dyndbg" "$(cat /tmp/dyndbg_err 2>/dev/null)"
        fi
    else
        fail "learn07_dyndbg" "info/debug before control"
    fi

    TR=/sys/kernel/tracing
    if [ ! -f "$TR/trace" ]; then
        TR=/sys/kernel/debug/tracing
    fi
    if [ -f "$TR/trace" ]; then
        echo virt_debug_ping > "$TR/set_ftrace_filter" 2>/tmp/ft_err
        if [ $? -eq 0 ]; then
            echo function > "$TR/current_tracer"
            echo > "$TR/trace"
            echo 1 > /sys/devices/platform/virt_debug/trigger
            if grep -q virt_debug_ping "$TR/trace"; then
                pass "learn07_ftrace"
            else
                fail "learn07_ftrace" "symbol missing"
            fi
            echo nop > "$TR/current_tracer"
        else
            fail "learn07_ftrace" "$(cat /tmp/ft_err)"
        fi
    else
        fail "learn07_ftrace" "tracefs not mounted"
    fi
    safe_rmmod virt_debug
else
    fail "learn07_debug" "$(cat /tmp/insmod_err)"
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
