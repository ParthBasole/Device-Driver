# Compiles the real driver-3.c against stub kernel headers and exercises the
# scancode decoder with recorded byte sequences. No kernel, no hardware, no
# root -- this is the part of the driver that is testable on any machine,
# including the one you wrote it on.

STUB_NAMES := module fs device uaccess interrupt io ioport wait spinlock \
              poll ctype version
STUBS := $(addprefix include/linux/,$(addsuffix .h,$(STUB_NAMES)))

# -Wno-unused-parameter because the kernel's callback signatures are fixed:
# kbd_open() must take (inode, filp) whether or not it reads them, and the
# kernel's own W=1 build does not enable that warning either.
CFLAGS := -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
          -I. -Iinclude -I..

run: decode_test
	./decode_test

decode_test: decode_test.c kstub.h ../driver-3.c | $(STUBS)
	$(CC) $(CFLAGS) -o $@ decode_test.c

# driver-3.c includes <linux/*.h>. Empty files satisfy the preprocessor;
# kstub.h supplies the handful of symbols the driver actually uses.
$(STUBS):
	@mkdir -p include/linux
	@touch $@

clean:
	$(RM) -r decode_test decode_test.dSYM include

.PHONY: run clean
