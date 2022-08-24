obj-m += gpio-device-driver-interrupt.o

KDIR = /home/luan/workspace/tmp/beaglebone/linux

all:
	make -C $(KDIR)  M=$(shell pwd) modules

clean:
	make -C $(KDIR)  M=$(shell pwd) clean
