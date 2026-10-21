KERNEL_DIR ?= /lib/modules/5.15.0-173-generic/build
PWD := $(shell pwd)

obj-m := virt_dt.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

dtb: virt-reg.dts
	dtc -I dts -O dtb -o virt-reg.dtb virt-reg.dts

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f virt-reg.dtb

.PHONY: all dtb clean
