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
	$(MAKE) -C tests clean
	$(RM) demo

# Every warning kbuild offers, plus sparse's __user address-space checking.
# C=2 is silently skipped if the `sparse` package is not installed.
check:
	$(MAKE) -C $(KDIR) M=$(CURDIR) W=1 C=2 KCFLAGS=-Werror modules

# Compile the driver against stub kernel headers and exercise the scancode
# decoder, ring buffer, read() and poll() in userspace. No kernel, no
# hardware, no root -- runs anywhere, including macOS.
test:
	$(MAKE) -C tests run

# Same strict build, but on x86_64 -- for when you are developing somewhere
# this module cannot be built natively (inb()/IRQ 1/port 0x60 are x86-only).
# Needs Docker; slow on Apple Silicon because the container is emulated.
docker-check:
	docker run --rm --platform linux/amd64 -v "$(CURDIR)":/src -w /src \
		ubuntu:24.04 bash -c 'set -e; export DEBIAN_FRONTEND=noninteractive; \
		apt-get update -qq >/dev/null; \
		apt-get install -y -qq build-essential linux-headers-generic sparse >/dev/null; \
		K=$$(ls /lib/modules | head -1); echo "building against $$K"; \
		make -C /lib/modules/$$K/build M=/src W=1 C=2 KCFLAGS=-Werror modules; \
		make -C /lib/modules/$$K/build M=/src clean'

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

.PHONY: all module clean check test docker-check unbind rebind load unload

endif
