obj-m += vmem_disk.o

all:
	make -C /lib/modules/5.15.0-173-generic/build M=$(PWD) modules

clean:
	make -C /lib/modules/5.15.0-173-generic/build M=$(PWD) clean
