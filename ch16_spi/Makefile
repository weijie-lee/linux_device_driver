obj-m += spi_master.o
obj-m += spi_slave.o

all:
	make -C /lib/modules/5.15.0-173-generic/build M=$(PWD) modules

clean:
	make -C /lib/modules/5.15.0-173-generic/build M=$(PWD) clean
