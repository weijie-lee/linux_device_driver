obj-m := dma_demo.o

all:
	make -C /lib/modules/5.15.0-173-generic/build M=$(shell pwd) modules
clean:
	make -C /lib/modules/5.15.0-173-generic/build M=$(shell pwd) clean
