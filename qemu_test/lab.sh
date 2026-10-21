#!/bin/sh
# 单章实验。由 init.sh 在内核参数 ldd_chapter= 时调用。
# 每条客户机命令先打印 "+ 命令"，再打印回显。成功以 [PASS] 结尾。

MODDIR=/lib/modules/driver_test
ok=1
LABEL=""

begin() {
    LABEL="$1"
    ok=1
    echo "======== $LABEL ========"
}

finish() {
    if [ "$ok" -eq 1 ]; then
        pass "$LABEL"
    else
        fail "$LABEL" "see commands above"
    fi
}

load() {
    echo "+ insmod $1"
    if insmod "$1" 2>/tmp/insmod_err; then
        echo "insmod ok"
        return 0
    fi
    echo "insmod failed: $(cat /tmp/insmod_err)"
    ok=0
    return 1
}

unload() {
    echo "+ rmmod $1"
    if rmmod "$1" 2>/tmp/rmmod_err; then
        echo "rmmod ok"
        return 0
    fi
    echo "rmmod failed: $(cat /tmp/rmmod_err)"
    ok=0
    return 1
}

run() {
    echo "+ $1"
    LAST=$(eval "$1" 2>&1) || true
    printf '%s\n' "$LAST"
}

want() {
    if printf '%s\n' "$LAST" | grep -F -q "$1"; then
        return 0
    fi
    echo "期望回显包含: $1"
    ok=0
    return 1
}

# 平台模块：加载后列出名字出现的 sysfs/dev 节点，并可选再跑一条取值命令。
plat() {
    label="$1"
    ko="$2"
    mod="$3"
    token="$4"
    extra="$5"
    needle="$6"
    begin "$label"
    load "$MODDIR/$ko" || { finish; return; }
    run "lsmod | grep $mod"
    want "$mod"
    run "find /sys /dev /proc -name '*${token}*' 2>/dev/null | head -n 30"
    if [ -n "$extra" ]; then
        run "$extra"
        want "$needle"
    else
        want "$token"
    fi
    unload "$mod"
    finish
}

lab_ch01() {
    begin "ch01_kfifo"
    load "$MODDIR/ch01_kernel_basics_kfifo_demo_static.ko" || { finish; return; }
    run "dmesg | grep fifo"
    want "fifo test begin"
    want "fifo test end"
    want "kfifo peek"
    run "dmesg | grep 'buf:'"
    want "buf:hello"
    echo "+ echo labfifo > /proc/bytestream-fifo"
    echo labfifo > /proc/bytestream-fifo
    run "cat /proc/bytestream-fifo"
    want "labfifo"
    unload kfifo_demo_static
    finish
}

lab_ch02() {
    begin "ch02_globalmem"
    load "$MODDIR/ch02_char_basic_globalmem.ko" || { finish; return; }
    run "ls /dev/global_mem_0 /dev/global_mem_1 /dev/global_mem_2 /dev/global_mem_3"
    want "global_mem_0"
    echo "+ echo test_data > /dev/global_mem_0"
    echo test_data > /dev/global_mem_0
    run "head -c 9 /dev/global_mem_0"
    want "test_data"
    unload globalmem
    finish
}

lab_ch03() {
    begin "ch03_globalfifo"
    load "$MODDIR/ch03_char_advanced_globalfifo.ko" || { finish; return; }
    run "ls /dev/global_mem_0"
    want "global_mem_0"
    echo "+ echo fifo_test > /dev/global_mem_0"
    echo fifo_test > /dev/global_mem_0
    run "head -c 9 /dev/global_mem_0"
    want "fifo_test"
    unload globalfifo
    finish
}

lab_ch04() {
    begin "ch04_second"
    load "$MODDIR/ch04_timer_second.ko" || { finish; return; }
    run "ls /dev/second"
    want "/dev/second"
    echo "+ exec 3<>/dev/second"
    exec 3<>/dev/second
    echo "+ sleep 2"
    sleep 2
    echo "+ head -c 4 <&3 > /tmp/sec"
    head -c 4 <&3 > /tmp/sec
    exec 3<&-
    run "od -An -tu4 /tmp/sec"
    set -- $LAST
    if [ "${1:-0}" -ge 1 ] 2>/dev/null; then
        :
    else
        echo "期望打开 /dev/second 两秒后读到的秒数 >= 1"
        ok=0
    fi
    unload second
    finish
}

lab_ch05() {
    begin "ch05_misc"
    load "$MODDIR/ch05_misc_misc_demo.ko" || { finish; return; }
    run "ls /dev/misc_demo"
    want "/dev/misc_demo"
    echo "+ echo miscdata > /dev/misc_demo"
    echo miscdata > /dev/misc_demo
    run "head -c 8 /dev/misc_demo"
    want "miscdata"
    unload misc_demo
    finish
}

lab_ch06() {
    begin "ch06_platform"
    load "$MODDIR/ch06_platform_platform_demo.ko" || { finish; return; }
    run "ls /dev/virt_plat /sys/bus/platform/devices"
    want "virt_plat"
    echo "+ echo hi > /dev/virt_plat"
    echo hi > /dev/virt_plat
    run "head -c 2 /dev/virt_plat"
    want "hi"
    unload platform_demo
    finish
}

lab_ch07() {
    begin "ch07_input"
    load "$MODDIR/ch07_input_input_demo.ko" || { finish; return; }
    run "ls /dev/input"
    want "event"
    echo "+ echo 'key 30' > /proc/virt_kbd"
    echo 'key 30' > /proc/virt_kbd
    run "dmesg | grep virt_kbd"
    want "reported key 30"
    unload input_demo
    finish
}

lab_ch08() {
    begin "ch08_regmap"
    load "$MODDIR/ch08_regmap_regmap_demo.ko" || { finish; return; }
    run "ls /dev/regmap_demo"
    want "/dev/regmap_demo"
    run "dmesg | grep regmap_demo"
    want "initialized"
    unload regmap_demo
    finish
}

lab_ch09() {
    begin "ch09_watchdog"
    load "$MODDIR/ch09_watchdog_watchdog_demo.ko" || { finish; return; }
    run "ls /dev/watchdog /dev/watchdog0"
    want "watchdog"
    run "dmesg | grep virt_wdt"
    want "registered"
    unload watchdog_demo
    finish
}

lab_ch10() {
    begin "ch10_rtc"
    load "$MODDIR/ch10_rtc_rtc_demo.ko" || { finish; return; }
    run "cat /sys/class/rtc/rtc*/name"
    want "virt_rtc"
    run "ls /dev/rtc /dev/rtc0"
    want "rtc"
    unload rtc_demo
    finish
}

lab_ch11() {
    begin "ch11_pwm"
    load "$MODDIR/ch11_pwm_pwm_demo.ko" || { finish; return; }
    run "ls /sys/class/pwm"
    want "pwmchip"
    run "dmesg | grep virt_pwm"
    want "PWM channels"
    unload pwm_demo
    finish
}

lab_ch12() {
    begin "ch12_dma"
    load "$MODDIR/ch12_dma_dma_demo.ko" || { finish; return; }
    run "dmesg | grep kmalloc"
    want "kmalloc virtual addr"
    unload dma_demo
    finish
}

lab_ch13() {
    begin "ch13_snull"
    load "$MODDIR/ch13_net_virtual_snull.ko" || { finish; return; }
    run "ip link"
    want "sn0"
    want "sn1"
    unload snull
    finish
}

lab_ch14() {
    begin "ch14_eth_mac"
    load "$MODDIR/ch14_net_mac_phy_eth_mac.ko" || { finish; return; }
    run "ip link"
    want "veth0_mac"
    unload eth_mac
    finish
}

lab_ch15() {
    begin "ch15_i2c"
    load "$MODDIR/ch15_i2c_i2c_master.ko" || { finish; return; }
    load "$MODDIR/ch15_i2c_i2c_slave.ko" || { unload i2c_master; finish; return; }
    run "ls /dev/i2c_virt"
    want "/dev/i2c_virt"
    run "dmesg | grep i2c_virt"
    want "registered as i2c-"
    echo "+ echo ab > /dev/i2c_virt"
    echo ab > /dev/i2c_virt
    run "dmesg | grep i2c_virt_slave"
    want "wrote"
    unload i2c_slave
    unload i2c_master
    finish
}

lab_ch16() {
    begin "ch16_spi"
    load "$MODDIR/ch16_spi_spi_master.ko" || { finish; return; }
    load "$MODDIR/ch16_spi_spi_slave.ko" || { unload spi_master; finish; return; }
    run "ls /dev/spi_virt"
    want "/dev/spi_virt"
    echo "+ echo sp > /dev/spi_virt"
    echo sp > /dev/spi_virt
    run "head -c 2 /dev/spi_virt"
    want "sp"
    unload spi_slave
    unload spi_master
    finish
}

lab_ch17() {
    begin "ch17_block"
    load "$MODDIR/ch17_block_vmem_disk.ko" || { finish; return; }
    run "ls /dev/vmem_disk0 /sys/block"
    want "vmem_disk"
    unload vmem_disk
    finish
}

lab_ch18() {
    begin "ch18_mmc"
    load "$MODDIR/ch18_mmc_mmc_virt.ko" || { finish; return; }
    sleep 1
    run "ls /sys/class/mmc_host /dev"
    want "mmc"
    unload mmc_virt
    finish
}

lab_pair() {
    label="$1"
    ko1="$2"
    mod1="$3"
    pat1="$4"
    ko2="$5"
    mod2="$6"
    pat2="$7"
    begin "$label"
    load "$MODDIR/$ko1" || { finish; return; }
    run "dmesg | grep -F '$pat1'"
    want "$pat1"
    run "ls /sys/bus/pci/drivers/$mod1 /sys/bus/usb/drivers/$mod1 2>/dev/null"
    unload "$mod1"
    load "$MODDIR/$ko2" || { finish; return; }
    run "dmesg | grep -F '$pat2'"
    want "$pat2"
    unload "$mod2"
    finish
}

lab_ch19() {
    lab_pair "ch19_pci" \
        ch19_pci_bus_pci_basic.ko pci_basic "PCI driver registered" \
        ch19_pci_bus_pci_advanced.ko pci_advanced "PCI driver registered"
}

lab_ch20() {
    begin "ch20_usb"
    load "$MODDIR/ch20_usb_bus_usb_basic.ko" || { finish; return; }
    run "dmesg | grep -F USB"
    want "registered"
    run "ls -d /sys/bus/usb/drivers/usb_basic"
    want "usb_basic"
    unload usb_basic
    load "$MODDIR/ch20_usb_bus_usb_advanced.ko" || { finish; return; }
    run "ls -d /sys/bus/usb/drivers/usb_advanced"
    want "usb_advanced"
    unload usb_advanced
    finish
}

lab_ch21() { plat ch21_gpio ch21_gpio_virt_gpio.ko virt_gpio virt_gpio "" ""; }
lab_ch22() { plat ch22_pinctrl ch22_pinctrl_virt_pinctrl.ko virt_pinctrl virt_pinctrl "" ""; }
lab_ch23() { plat ch23_clk ch23_clk_virt_clk.ko virt_clk virt_fixed_24m "dmesg | grep virt_fixed_24m" "24 MHz"; }
lab_ch24() { plat ch24_regulator ch24_regulator_virt_regulator.ko virt_regulator virt_vdd_1v8 "cat /sys/class/regulator/regulator*/microvolts" "1800000"; }
lab_ch25() { plat ch25_reset ch25_reset_virt_reset.ko virt_reset virt_reset "" ""; }
lab_ch26() { plat ch26_nvmem ch26_nvmem_virt_nvmem.ko virt_nvmem virt-nvmem "" ""; }
lab_ch27() { plat ch27_hwmon ch27_hwmon_virt_hwmon.ko virt_hwmon virt_hwmon "cat /sys/class/hwmon/hwmon*/temp1_input" "25000"; }
lab_ch28() { plat ch28_leds ch28_leds_virt_led.ko virt_led virt:status "cat /sys/class/leds/virt:status/max_brightness" "255"; }
lab_ch29() { plat ch29_thermal ch29_thermal_virt_thermal.ko virt_thermal virt_thermal "cat /sys/class/thermal/thermal_zone*/type" "virt_thermal"; }
lab_ch30() { plat ch30_power ch30_power_virt_psy.ko virt_psy virt-battery "cat /sys/class/power_supply/virt-battery/capacity" "77"; }
lab_ch31() { plat ch31_backlight ch31_backlight_virt_backlight.ko virt_backlight virt_backlight "" ""; }
lab_ch32() { plat ch32_phy ch32_phy_virt_phy.ko virt_phy virt_phy "" ""; }
lab_ch33() { plat ch33_pps ch33_pps_virt_pps.ko virt_pps pps "" ""; }
lab_ch34() { plat ch34_ptp ch34_ptp_virt_ptp.ko virt_ptp ptp "" ""; }
lab_ch35() { plat ch35_tty ch35_tty_virt_tty.ko virt_tty ttyVIRT "" ""; }
lab_ch36() {
    begin "ch36_fb"
    load "$MODDIR/ch36_fb_virt_fb.ko" || { finish; return; }
    run "cat /sys/class/graphics/fb*/name"
    want "virtfb"
    echo "+ echo 0 > /sys/class/vtconsole/vtcon1/bind"
    echo 0 > /sys/class/vtconsole/vtcon1/bind 2>/dev/null || true
    echo "+ echo 0 > /sys/class/vtconsole/vtcon0/bind"
    echo 0 > /sys/class/vtconsole/vtcon0/bind 2>/dev/null || true
    unload virt_fb
    finish
}
lab_ch37() { plat ch37_mfd ch37_mfd_virt_mfd.ko virt_mfd virt-mfd "" ""; }
lab_ch38() { plat ch38_extcon ch38_extcon_virt_extcon.ko virt_extcon virt_extcon "" ""; }
lab_ch39() { plat ch39_scsi ch39_scsi_virt_scsi.ko virt_scsi virt_scsi "" ""; }
lab_ch40() { plat ch40_crypto ch40_crypto_virt_crypto.ko virt_crypto virt_crypto "grep virt-null /proc/crypto" "virt-null"; }
lab_ch41() { plat ch41_virtio ch41_virtio_virt_virtio.ko virt_virtio virt_virtio "" ""; }
lab_ch42() { plat ch42_remoteproc ch42_remoteproc_virt_rproc.ko virt_rproc virt_rproc "cat /sys/class/remoteproc/remoteproc*/name" "virt-rproc"; }
lab_ch43() { plat ch43_devfreq ch43_devfreq_virt_devfreq.ko virt_devfreq virt_devfreq "" ""; }
lab_ch44() { plat ch44_dmaengine ch44_dmaengine_virt_dmaeng.ko virt_dmaeng virt_dmaeng "" ""; }
lab_ch45() { plat ch45_rfkill ch45_rfkill_virt_rfkill.ko virt_rfkill virt_rfkill "" ""; }
lab_ch46() { plat ch46_hwspinlock ch46_hwspinlock_virt_hwspin.ko virt_hwspin virt_hwspin "" ""; }
lab_ch47() { plat ch47_mailbox ch47_mailbox_virt_mbox.ko virt_mbox virt_mbox "" ""; }
lab_ch48() { plat ch48_powercap ch48_powercap_virt_powercap.ko virt_powercap virt_pcap "" ""; }
lab_ch49() { plat ch49_opp ch49_opp_virt_opp.ko virt_opp virt_opp "" ""; }
lab_ch50() { plat ch50_mdio ch50_mdio_virt_mdio.ko virt_mdio virt-mdio "" ""; }
lab_ch51() { plat ch51_iio ch51_iio_virt_iio.ko virt_iio virt_iio "cat /sys/bus/iio/devices/iio:device*/in_voltage0_raw" "1234"; }
lab_ch52() { plat ch52_alsa ch52_alsa_virt_alsa.ko virt_alsa virt "cat /proc/asound/cards" "virt"; }
lab_ch53() { plat ch53_hid ch53_hid_virt_hid.ko virt_hid virt_hid "" ""; }
lab_ch54() { plat ch54_mtd ch54_mtd_virt_mtd.ko virt_mtd virt-mtd "cat /sys/class/mtd/mtd*/name" "virt-mtd"; }
lab_ch55() { plat ch55_uio ch55_uio_virt_uio.ko virt_uio virt_uio "" ""; }
lab_ch56() { plat ch56_w1 ch56_w1_virt_w1.ko virt_w1 virt_w1 "" ""; }
lab_ch57() { plat ch57_can ch57_can_virt_can.ko virt_can can "ip link" "can"; }
lab_ch58() { plat ch58_mux ch58_mux_virt_mux.ko virt_mux virt_mux "" ""; }
lab_ch59() { plat ch59_fpga ch59_fpga_virt_fpga.ko virt_fpga virt_fpga "cat /sys/class/fpga_manager/*/name" "virt-fpga"; }
lab_ch60() { plat ch60_spmi ch60_spmi_virt_spmi.ko virt_spmi virt_spmi "" ""; }
lab_ch61() { plat ch61_i3c ch61_i3c_virt_i3c.ko virt_i3c virt_i3c "" ""; }
lab_ch62() { plat ch62_gnss ch62_gnss_virt_gnss.ko virt_gnss virt_gnss "" ""; }
lab_ch63() { plat ch63_nfc ch63_nfc_virt_nfc.ko virt_nfc virt_nfc "" ""; }
lab_ch64() { plat ch64_bluetooth ch64_bluetooth_virt_bt.ko virt_bt virt_bt "" ""; }
lab_ch65() { plat ch65_v4l2 ch65_v4l2_virt_v4l2.ko virt_v4l2 virt_v4l2 "" ""; }
lab_ch66() { plat ch66_counter ch66_counter_virt_counter.ko virt_counter virt_count "" ""; }
lab_ch67() { plat ch67_drm ch67_drm_virt_drm.ko virt_drm virt_drm "" ""; }
lab_ch68() { plat ch68_interconnect ch68_interconnect_virt_icc.ko virt_icc virt_icc "" ""; }

lab_hardcode() {
    begin "hardcode"
    load "$MODDIR/hardcode_hardcode_samples.ko" || { finish; return; }
    run "dmesg | grep hardcode"
    want "hardcode samples ok"
    want "hardcode list key="
    unload hardcode_samples
    finish
}

lab_learn01() {
    begin "learn01_uapi"
    load "$MODDIR/learn_01_uapi_virt_uapi.ko" || { finish; return; }
    run "/bin/uapi_test"
    want "uapi ok: all"
    unload virt_uapi
    finish
}

lab_learn02() {
    begin "learn02_devicetree"
    load "$MODDIR/learn_02_devicetree_virt_dt.ko" || { finish; return; }
    run "cat /sys/devices/platform/virt_dt/threshold /sys/devices/platform/virt_dt/id /sys/devices/platform/virt_dt/irq"
    want "40"
    want "4c444454"
    want "19"
    unload virt_dt
    finish
}

lab_learn03() {
    begin "learn03_irq"
    load "$MODDIR/learn_03_irq_virt_irq.ko" || { finish; return; }
    echo "+ echo 1 > /sys/devices/platform/virt_irq/kick"
    echo 1 > /sys/devices/platform/virt_irq/kick
    sleep 1
    run "cat /sys/devices/platform/virt_irq/counts"
    set -- $LAST
    if [ "$1" -ge 1 ] 2>/dev/null && [ "$2" -ge 1 ] && [ "$3" -ge 1 ] && \
       [ "$4" -ge 1 ] && [ "$5" -ge 1 ] && [ "$6" -eq 0 ]; then
        :
    else
        echo "期望 counts 前五项 >= 1 且 raise_errno 为 0"
        ok=0
    fi
    unload virt_irq
    finish
}

lab_learn04() {
    begin "learn04_sensor"
    load "$MODDIR/learn_04_soc_sensor_virt_sensor.ko" || { finish; return; }
    echo "+ echo 37 > /sys/devices/platform/virt_sensor/temperature"
    echo 37 > /sys/devices/platform/virt_sensor/temperature
    sleep 1
    run "cat /sys/devices/platform/virt_sensor/temperature /sys/devices/platform/virt_sensor/clock_hz /sys/devices/platform/virt_sensor/irq_count /sys/devices/platform/virt_sensor/last_temp"
    want "37"
    want "24000000"
    unload virt_sensor
    finish
}

lab_learn05() {
    begin "learn05_pm"
    load "$MODDIR/learn_05_pm_virt_pm.ko" || { finish; return; }
    echo "+ echo suspend > /sys/devices/platform/virt_pm/sleep"
    echo suspend > /sys/devices/platform/virt_pm/sleep
    run "cat /sys/devices/platform/virt_pm/ctrl /sys/devices/platform/virt_pm/sleeping"
    want "0"
    echo "+ echo resume > /sys/devices/platform/virt_pm/sleep"
    echo resume > /sys/devices/platform/virt_pm/sleep
    run "cat /sys/devices/platform/virt_pm/ctrl"
    want "3"
    echo "+ echo 1 > /sys/devices/platform/virt_pm/idle"
    echo 1 > /sys/devices/platform/virt_pm/idle
    run "cat /sys/devices/platform/virt_pm/power/runtime_status /sys/devices/platform/virt_pm/runtime_marker"
    want "suspended"
    echo "+ echo on > /sys/devices/platform/virt_pm/power/control"
    echo on > /sys/devices/platform/virt_pm/power/control
    run "cat /sys/devices/platform/virt_pm/power/runtime_status /sys/devices/platform/virt_pm/runtime_marker"
    want "active"
    unload virt_pm
    finish
}

lab_learn06() {
    begin "learn06_dma"
    load "$MODDIR/learn_06_dma_virt_dma.ko" || { finish; return; }
    run "cat /sys/devices/platform/virt_dma/report"
    want "coherent:ok"
    want "stream:ok"
    want "sg:ok"
    unload virt_dma
    finish
}

lab_learn07() {
    begin "learn07_debug"
    load "$MODDIR/learn_07_debug_virt_debug.ko" || { finish; return; }
    echo "+ echo 1 > /sys/devices/platform/virt_debug/trigger"
    echo 1 > /sys/devices/platform/virt_debug/trigger
    run "dmesg | grep virt_debug"
    want "virt_debug: ping info"
    if printf '%s\n' "$LAST" | grep -F -q "virt_debug: ping debug"; then
        echo "打开 dyndbg 之前不应出现 ping debug"
        ok=0
    fi
    echo "+ echo 'module virt_debug +p' > /sys/kernel/debug/dynamic_debug/control"
    echo 'module virt_debug +p' > /sys/kernel/debug/dynamic_debug/control
    echo "+ echo 1 > /sys/devices/platform/virt_debug/trigger"
    echo 1 > /sys/devices/platform/virt_debug/trigger
    run "dmesg | grep 'ping debug'"
    want "virt_debug: ping debug"
    TR=/sys/kernel/tracing
    if [ ! -f "$TR/trace" ]; then
        TR=/sys/kernel/debug/tracing
    fi
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    echo "+ echo nop > $TR/current_tracer"
    echo nop > "$TR/current_tracer"
    echo "+ echo function > $TR/current_tracer"
    echo function > "$TR/current_tracer"
    echo "+ echo virt_debug_ping > $TR/set_ftrace_filter"
    echo virt_debug_ping > "$TR/set_ftrace_filter"
    echo "+ echo > $TR/trace"
    echo > "$TR/trace"
    echo "+ echo 1 > $TR/tracing_on"
    echo 1 > "$TR/tracing_on"
    echo "+ echo 1 > /sys/devices/platform/virt_debug/trigger"
    echo 1 > /sys/devices/platform/virt_debug/trigger
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    run "grep virt_debug_ping $TR/trace"
    want "virt_debug_ping"
    echo nop > "$TR/current_tracer"
    echo 1 > "$TR/tracing_on"
    unload virt_debug
    finish
}

lab_main() {
    case "$1" in
        all)
            lab_ch01; lab_ch02; lab_ch03; lab_ch04; lab_ch05; lab_ch06
            lab_ch07; lab_ch08; lab_ch09; lab_ch10; lab_ch11; lab_ch12
            lab_ch13; lab_ch14; lab_ch15; lab_ch16; lab_ch17; lab_ch18
            lab_ch19; lab_ch20; lab_ch21; lab_ch22; lab_ch23; lab_ch24
            lab_ch25; lab_ch26; lab_ch27; lab_ch28; lab_ch29; lab_ch30
            lab_ch31; lab_ch32; lab_ch33; lab_ch34; lab_ch35; lab_ch36
            lab_ch37; lab_ch38; lab_ch39; lab_ch40; lab_ch41; lab_ch42
            lab_ch43; lab_ch44; lab_ch45; lab_ch46; lab_ch47; lab_ch48
            lab_ch49; lab_ch50; lab_ch51; lab_ch52; lab_ch53; lab_ch54
            lab_ch55; lab_ch56; lab_ch57; lab_ch58; lab_ch59; lab_ch60
            lab_ch61; lab_ch62; lab_ch63; lab_ch64; lab_ch65; lab_ch66
            lab_ch67; lab_ch68
            lab_hardcode
            lab_learn01; lab_learn02; lab_learn03; lab_learn04
            lab_learn05; lab_learn06; lab_learn07
            ;;
        ch01|ch01_kernel_basics) lab_ch01 ;;
        ch02|ch02_char_basic) lab_ch02 ;;
        ch03|ch03_char_advanced) lab_ch03 ;;
        ch04|ch04_timer) lab_ch04 ;;
        ch05|ch05_misc) lab_ch05 ;;
        ch06|ch06_platform) lab_ch06 ;;
        ch07|ch07_input) lab_ch07 ;;
        ch08|ch08_regmap|ch04_regmap_framework) lab_ch08 ;;
        ch09|ch09_watchdog) lab_ch09 ;;
        ch10|ch10_rtc) lab_ch10 ;;
        ch11|ch11_pwm) lab_ch11 ;;
        ch12|ch12_dma) lab_ch12 ;;
        ch13|ch13_net_virtual) lab_ch13 ;;
        ch14|ch14_net_mac_phy) lab_ch14 ;;
        ch15|ch15_i2c|ch05_i2c_bus) lab_ch15 ;;
        ch16|ch16_spi) lab_ch16 ;;
        ch17|ch17_block) lab_ch17 ;;
        ch18|ch18_mmc) lab_ch18 ;;
        ch19|ch19_pci_bus|ch07_pci_bus) lab_ch19 ;;
        ch20|ch20_usb_bus|ch08_usb_bus) lab_ch20 ;;
        ch21|ch21_gpio) lab_ch21 ;;
        ch22|ch22_pinctrl) lab_ch22 ;;
        ch23|ch23_clk) lab_ch23 ;;
        ch24|ch24_regulator) lab_ch24 ;;
        ch25|ch25_reset) lab_ch25 ;;
        ch26|ch26_nvmem) lab_ch26 ;;
        ch27|ch27_hwmon) lab_ch27 ;;
        ch28|ch28_leds) lab_ch28 ;;
        ch29|ch29_thermal) lab_ch29 ;;
        ch30|ch30_power) lab_ch30 ;;
        ch31|ch31_backlight) lab_ch31 ;;
        ch32|ch32_phy) lab_ch32 ;;
        ch33|ch33_pps) lab_ch33 ;;
        ch34|ch34_ptp) lab_ch34 ;;
        ch35|ch35_tty) lab_ch35 ;;
        ch36|ch36_fb) lab_ch36 ;;
        ch37|ch37_mfd) lab_ch37 ;;
        ch38|ch38_extcon) lab_ch38 ;;
        ch39|ch39_scsi) lab_ch39 ;;
        ch40|ch40_crypto) lab_ch40 ;;
        ch41|ch41_virtio) lab_ch41 ;;
        ch42|ch42_remoteproc) lab_ch42 ;;
        ch43|ch43_devfreq) lab_ch43 ;;
        ch44|ch44_dmaengine) lab_ch44 ;;
        ch45|ch45_rfkill) lab_ch45 ;;
        ch46|ch46_hwspinlock) lab_ch46 ;;
        ch47|ch47_mailbox) lab_ch47 ;;
        ch48|ch48_powercap) lab_ch48 ;;
        ch49|ch49_opp) lab_ch49 ;;
        ch50|ch50_mdio) lab_ch50 ;;
        ch51|ch51_iio) lab_ch51 ;;
        ch52|ch52_alsa) lab_ch52 ;;
        ch53|ch53_hid) lab_ch53 ;;
        ch54|ch54_mtd) lab_ch54 ;;
        ch55|ch55_uio) lab_ch55 ;;
        ch56|ch56_w1) lab_ch56 ;;
        ch57|ch57_can) lab_ch57 ;;
        ch58|ch58_mux) lab_ch58 ;;
        ch59|ch59_fpga) lab_ch59 ;;
        ch60|ch60_spmi) lab_ch60 ;;
        ch61|ch61_i3c) lab_ch61 ;;
        ch62|ch62_gnss) lab_ch62 ;;
        ch63|ch63_nfc) lab_ch63 ;;
        ch64|ch64_bluetooth) lab_ch64 ;;
        ch65|ch65_v4l2) lab_ch65 ;;
        ch66|ch66_counter) lab_ch66 ;;
        ch67|ch67_drm) lab_ch67 ;;
        ch68|ch68_interconnect) lab_ch68 ;;
        hardcode) lab_hardcode ;;
        learn01|learn/01_uapi|learn/01) lab_learn01 ;;
        learn02|learn/02_devicetree|learn/02) lab_learn02 ;;
        learn03|learn/03_irq|learn/03) lab_learn03 ;;
        learn04|learn/04_soc_sensor|learn/04) lab_learn04 ;;
        learn05|learn/05_pm|learn/05) lab_learn05 ;;
        learn06|learn/06_dma|learn/06) lab_learn06 ;;
        learn07|learn/07_debug|learn/07) lab_learn07 ;;
        *)
            echo "未知章节: $1"
            fail "lab" "unknown chapter $1"
            ;;
    esac
}
