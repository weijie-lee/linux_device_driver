# Linux 设备驱动开发实践仓库 — 顶层 Makefile
# 按章节编号组织，ch00_framework 为驱动框架总览文档章节，不参与编译

KERNEL_DIR ?= /lib/modules/5.15.0-173-generic/build
PWD := $(shell pwd)

# 所有可编译目录。kernel/ 是驱动框架拆解，不和 ch* 的具体驱动混在一条学习顺序里。
CHAPTERS := \
	ch01_kernel_basics \
	ch02_char_basic \
	ch03_char_advanced \
	ch04_timer \
	ch05_misc \
	ch06_platform \
	ch07_input \
	ch08_regmap \
	ch09_watchdog \
	ch10_rtc \
	ch11_pwm \
	ch12_dma \
	ch13_net_virtual \
	ch14_net_mac_phy \
	ch15_i2c \
	ch16_spi \
	ch17_block \
	ch18_mmc \
	ch19_pci_bus \
	ch20_usb_bus \
	ch21_gpio \
	ch22_pinctrl \
	ch23_clk \
	ch24_regulator \
	ch25_reset \
	ch26_nvmem \
	ch27_hwmon \
	ch28_leds \
	ch29_thermal \
	ch30_power \
	ch31_backlight \
	ch32_phy \
	ch33_pps \
	ch34_ptp \
	ch35_tty \
	ch36_fb \
	ch37_mfd \
	ch38_extcon \
	ch39_scsi \
	ch40_crypto \
	ch41_virtio \
	ch42_remoteproc \
	ch43_devfreq \
	ch44_dmaengine \
	ch45_rfkill \
	ch46_hwspinlock \
	ch47_mailbox \
	ch48_powercap \
	ch49_opp \
	ch50_mdio \
	ch51_iio \
	ch52_alsa \
	ch53_hid \
	ch54_mtd \
	ch55_uio \
	ch56_w1 \
	ch57_can \
	ch58_mux \
	ch59_fpga \
	ch60_spmi \
	ch61_i3c \
	ch62_gnss \
	ch63_nfc \
	ch64_bluetooth \
	ch65_v4l2 \
	ch66_counter \
	ch67_drm \
	ch68_interconnect \
	hardcode \
	learn/01_uapi \
	learn/02_devicetree \
	learn/03_irq \
	learn/04_soc_sensor \
	learn/05_pm \
	learn/06_dma \
	learn/07_debug \
	kernel/01_driver_framework

.PHONY: all clean test lab help $(CHAPTERS)

all: $(CHAPTERS)

$(CHAPTERS):
	@echo ">>> Building $@ ..."
	+$(MAKE) -C $(KERNEL_DIR) M=$(abspath $(CURDIR)/$@) modules

clean:
	@for dir in $(CHAPTERS); do \
		if [ -d $$dir ]; then \
			echo ">>> Cleaning $$dir ..."; \
			rm -f $$dir/*.o $$dir/*.ko $$dir/*.mod.c $$dir/*.mod \
			      $$dir/*.symvers $$dir/*.order $$dir/.*.cmd; \
			rm -rf $$dir/.tmp_versions; \
		fi; \
	done

# 编译全部模块，在 5.15 的 QEMU 客户机里加载并检查（模块不在宿主机上 insmod）
test:
	bash qemu_test/qemu_test_all.sh

# 每章的命令、回显和成功判据：make lab CHAPTER=ch21，缺省跑全部章节
lab:
	bash qemu_test/run_chapter.sh $(or $(CHAPTER),all)

help:
	@echo "Linux 设备驱动开发实践仓库"
	@echo ""
	@echo "用法："
	@echo "  make all          编译所有章节"
	@echo "  make clean        清理所有编译产物"
	@echo "  make test         在 QEMU 客户机里加载全部模块并检查"
	@echo "  make lab CHAPTER=ch21  在客户机里跑一章，打印命令和回显"
	@echo "  make lab CHAPTER=kernel01  跑驱动框架拆解"
	@echo "  make ch02_char_basic  编译指定章节"
	@echo ""
	@echo "章节列表："
	@for dir in $(CHAPTERS); do echo "  $$dir"; done
