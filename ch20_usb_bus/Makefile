KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
KBUILD_EXTRA_SYMBOLS ?=

obj-m := usb_basic.o usb_advanced.o

usb_basic-objs := basic/usb_basic.o
usb_advanced-objs := advanced/usb_advanced.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install

.PHONY: all clean install
