KERNEL_DIR ?= /lib/modules/5.15.0-173-generic/build
PWD := $(shell pwd)

obj-m := virt_sensor.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

dtb: virt-sensor.dts
	dtc -I dts -O dtb -o virt-sensor.dtb virt-sensor.dts

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f virt-sensor.dtb

.PHONY: all dtb clean
