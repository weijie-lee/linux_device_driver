KERNEL_DIR ?= /lib/modules/5.15.0-173-generic/build
PWD := $(shell pwd)

obj-m := virt_upath.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

# 对 sparse/bad_user.c 跑 sparse（C=2：不管是否需要重新编译都检查）。
sparse:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD)/sparse C=2 modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD)/sparse clean

.PHONY: all sparse clean
