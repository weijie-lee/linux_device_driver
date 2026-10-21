KERNEL_DIR ?= /lib/modules/5.15.0-173-generic/build
PWD := $(shell pwd)

obj-m := virt_uapi.o

all: modules uapi_test

modules:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

uapi_test: uapi_test.c virt_uapi_cmd.h
	$(CC) -static -O2 -Wall -o $@ uapi_test.c

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f uapi_test

.PHONY: all modules clean
