obj-m += hello_lkm.o
obj-m += gpio_lkm.o

KDIR := /lib/modules/$(shell uname -r)/build

all: modules gpio

modules:
	make -C $(KDIR) M=$(PWD) modules

gpio: gpio.c
	gcc -o gpio gpio.c

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f gpio
