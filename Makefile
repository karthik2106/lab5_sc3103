obj-m += alert_lkm.o

KDIR := /lib/modules/$(shell uname -r)/build

all: modules alert_user

modules:
	make -C $(KDIR) M=$(PWD) modules

alert_user: alert_user.c
	gcc -o alert_user alert_user.c

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f alert_user
