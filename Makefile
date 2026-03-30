# Linux 设备驱动开发实践仓库 — 顶层 Makefile
# 按章节编号组织，ch00_framework 为驱动框架总览文档章节，不参与编译

KERNEL_DIR ?= /lib/modules/5.15.0-173-generic/build
PWD := $(shell pwd)

# 所有可编译章节（按学习顺序排列）
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
	ch17_block \
	ch18_mmc
# ch16_spi disabled - needs SPI API fixes for Linux 6.8

.PHONY: all clean test help $(CHAPTERS)

all: $(CHAPTERS)

$(CHAPTERS):
	@echo ">>> Building $@ ..."
	@$(MAKE) -C $@ KERNEL_DIR=$(KERNEL_DIR) 2>/dev/null || \
		$(MAKE) -C $(KERNEL_DIR) M=$(PWD)/$@ modules

clean:
	@for dir in $(CHAPTERS); do \
		if [ -d $$dir ]; then \
			echo ">>> Cleaning $$dir ..."; \
			rm -f $$dir/*.o $$dir/*.ko $$dir/*.mod.c $$dir/*.mod \
			      $$dir/*.symvers $$dir/*.order $$dir/.*.cmd; \
			rm -rf $$dir/.tmp_versions; \
		fi; \
	done

# 运行所有章节的自动化测试
test:
	@PASS=0; FAIL=0; \
	for dir in $(CHAPTERS); do \
		if [ -f $$dir/tests/test.sh ]; then \
			echo ""; \
			echo "=== Testing: $$dir ==="; \
			if cd $$dir && sudo bash tests/test.sh; then \
				PASS=$$((PASS+1)); \
			else \
				FAIL=$$((FAIL+1)); \
			fi; \
			cd ..; \
		fi; \
	done; \
	echo ""; \
	echo "=== Test Summary: PASS=$$PASS  FAIL=$$FAIL ==="

help:
	@echo "Linux 设备驱动开发实践仓库"
	@echo ""
	@echo "用法："
	@echo "  make all          编译所有章节"
	@echo "  make clean        清理所有编译产物"
	@echo "  make test         运行所有章节的自动化测试"
	@echo "  make ch02_char_basic  编译指定章节"
	@echo ""
	@echo "章节列表："
	@for dir in $(CHAPTERS); do echo "  $$dir"; done
