# Two-pass external-module Makefile (Documentation/kbuild/modules.rst): kbuild
# re-enters this file with KERNELRELEASE set and reads only obj-m, so the
# convenience targets below can never collide with kbuild's own.
ifneq ($(KERNELRELEASE),)

obj-m := driver-3.o

else

KDIR := /lib/modules/$(shell uname -r)/build

all: module demo

module:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

demo: demo.c
	$(CC) -Wall -Wextra -O2 -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	$(RM) demo

# Hand IRQ 1 + ports 0x60/0x64 over from the built-in i8042 driver. Your PS/2
# keyboard stops working until `make rebind` -- do this in a VM or over SSH.
unbind:
	sudo sh -c 'echo -n i8042 > /sys/bus/platform/drivers/i8042/unbind'

rebind:
	sudo sh -c 'echo -n i8042 > /sys/bus/platform/drivers/i8042/bind'

load: module
	sudo insmod driver-3.ko

unload:
	sudo rmmod driver-3

.PHONY: all module clean unbind rebind load unload

endif
