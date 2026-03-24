KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
obj-m := platform_demo.o
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
.PHONY: all clean
