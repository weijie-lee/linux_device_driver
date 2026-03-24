KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# driver_template 仅作为教学示例，不参与实际编译
# 如需编译，取消下面的注释
# obj-m := driver_template.o

all:
	@echo "framework/ 目录为教学文档，不包含可编译的内核模块。"
	@echo "请参阅 README.md 了解 Linux 驱动框架的完整说明。"
	@echo "参考模板文件：driver_template.c"

clean:
	rm -f *.o *.ko *.mod.c *.mod *.symvers *.order .*.cmd
	rm -rf .tmp_versions

.PHONY: all clean
