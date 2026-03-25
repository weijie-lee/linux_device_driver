KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
KBUILD_EXTRA_SYMBOLS ?=

obj-m := pci_basic.o pci_advanced.o

pci_basic-objs := basic/pci_basic.o
pci_advanced-objs := advanced/pci_advanced.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install

.PHONY: all clean install
