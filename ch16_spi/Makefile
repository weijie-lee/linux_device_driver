KERNEL_DIR ?= /lib/modules/5.15.0-173-generic/build
obj-m += spi_master.o
obj-m += spi_slave.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

.PHONY: all clean
