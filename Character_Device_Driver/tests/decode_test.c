// Exercises the real kbd_decode(), ring buffer and kbd_read() from driver-3.c
// against recorded PS/2 scancode sequences. Run with `make -C tests`.
//
// Scancodes below are set 1: a make code, then the same code | 0x80 for the
// break. Sequences were taken from the AT/PS2 set-1 tables, including the
// awkward ones (0xe0 extended, the 0xe1 six-byte Pause, and the fake shift
// that arrows emit while Num Lock is on).

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "kstub.h"
#include "driver-3.c"		/* the driver, verbatim */

static int fails;

// kbd_decode() keeps shift/caps/num/ext in function-local statics, so a case
// running after another would inherit its modifier state. Fork per case: the
// child starts from the program's initial statics every time.
static void feed(const u8 *sc, int n, char *out)
{
	int k = 0;

	head = tail = 0;
	for (int i = 0; i < n; i++)
		kbd_decode(sc[i]);
	while (head != tail) {
		out[k++] = ring[tail];
		tail = (tail + 1) & (BUF_SIZE - 1);
	}
	out[k] = 0;
}

static void check(const char *name, const u8 *sc, int n, const char *want)
{
	char got[512] = {0};
	int fd[2], st;
	ssize_t r;
	pid_t pid;
	bool ok;

	if (pipe(fd)) {
		perror("pipe");
		exit(2);
	}
	pid = fork();
	if (pid == 0) {
		char buf[512];

		close(fd[0]);
		feed(sc, n, buf);
		if (write(fd[1], buf, strlen(buf)) < 0)
			_exit(3);
		_exit(0);
	}
	close(fd[1]);
	r = read(fd[0], got, sizeof got - 1);
	got[r < 0 ? 0 : r] = 0;
	close(fd[0]);
	waitpid(pid, &st, 0);

	ok = strcmp(got, want) == 0;
	if (!ok)
		fails++;
	printf("  %s  %-36s want=%-10s got=%s\n",
	       ok ? "pass" : "FAIL", name, want, got);
}

static void ok(const char *name, bool cond)
{
	if (!cond)
		fails++;
	printf("  %s  %s\n", cond ? "pass" : "FAIL", name);
}

#define T(name, want, ...) do {				\
	static const u8 _s[] = { __VA_ARGS__ };		\
	check(name, _s, sizeof _s, want);		\
} while (0)

int main(void)
{
	puts("\ntyping");
	T("h,i make+break",           "hi",    0x23,0xa3, 0x17,0x97);
	T("hello",                    "hello", 0x23,0xa3,0x12,0x92,0x26,0xa6,
	                                       0x26,0xa6,0x18,0x98);
	T("enter -> newline",         "\n",    0x1c,0x9c);
	T("backspace",                "\b",    0x0e,0x8e);
	T("space",                    " ",     0x39,0xb9);

	puts("\nshift");
	T("shift+a = A",              "A",  0x2a, 0x1e,0x9e, 0xaa);
	T("shift+1 = !",              "!",  0x2a, 0x02,0x82, 0xaa);
	T("right shift works too",    "A",  0x36, 0x1e,0x9e, 0xb6);
	T("release restores case",    "Aa", 0x2a,0x1e,0x9e,0xaa, 0x1e,0x9e);

	puts("\ncaps lock");
	T("caps -> A",                "A",  0x3a,0xba, 0x1e,0x9e);
	T("caps+shift inverts to a",  "a",  0x3a,0xba, 0x2a,0x1e,0x9e,0xaa);
	T("caps leaves digits alone", "1",  0x3a,0xba, 0x02,0x82);
	T("caps toggles off",         "aa", 0x3a,0xba, 0x3a,0xba, 0x1e,0x9e, 0x1e,0x9e);

	puts("\nextended (0xe0)");
	T("up arrow is not a char",   "",   0xe0,0x48, 0xe0,0xc8);
	// The regression the 0xe0 tracking exists for: arrows bracket themselves
	// in a fake shift. Mishandled, every later keystroke comes out capitalised.
	T("arrow fake-shift discarded", "a", 0xe0,0x2a, 0xe0,0x48, 0xe0,0xc8,
	                                     0xe0,0xaa, 0x1e,0x9e);
	T("keypad enter",             "\n", 0xe0,0x1c, 0xe0,0x9c);
	T("keypad slash",             "/",  0xe0,0x35, 0xe0,0xb5);

	puts("\npause (six bytes, 0xe1)");
	T("pause is not a char",      "",  0xe1,0x1d,0x45, 0xe1,0x9d,0xc5);
	// 0x45 inside Pause is also the Num Lock code -- it must not toggle.
	T("pause does not hit numlock", "7", 0xe1,0x1d,0x45, 0xe1,0x9d,0xc5,
	                                     0x47,0xc7);

	puts("\nnumeric keypad");
	T("7 8 9",                    "789", 0x47,0xc7, 0x48,0xc8, 0x49,0xc9);
	T("4 5 6",                    "456", 0x4b,0xcb, 0x4c,0xcc, 0x4d,0xcd);
	T("1 2 3",                    "123", 0x4f,0xcf, 0x50,0xd0, 0x51,0xd1);
	T("0 and .",                  "0.",  0x52,0xd2, 0x53,0xd3);
	T("+ and -",                  "+-",  0x4e,0xce, 0x4a,0xca);
	T("* (0x37)",                 "*",   0x37,0xb7);
	T("numlock off drops digits", "",    0x45,0xc5, 0x47,0xc7, 0x52,0xd2);
	T("numlock off keeps +/-",    "+-",  0x45,0xc5, 0x4e,0xce, 0x4a,0xca);
	T("numlock back on",          "7",   0x45,0xc5, 0x45,0xc5, 0x47,0xc7);
	T("shift must not alter kp",  "7",   0x2a, 0x47,0xc7, 0xaa);

	puts("\nunmapped keys are dropped");
	T("F1..F3",                   "", 0x3b,0xbb, 0x3c,0xbc, 0x3d,0xbd);
	T("escape",                   "", 0x01,0x81);
	T("left ctrl",                "", 0x1d,0x9d);

	puts("\nring buffer");
	head = tail = 0;
	for (int i = 0; i < BUF_SIZE * 4; i++)
		ring_push('x');
	ok("overflow caps at BUF_SIZE-1, no wrap corruption",
	   ((head - tail) & (BUF_SIZE - 1)) == BUF_SIZE - 1);

	puts("\nread()");
	{
		struct file f = { .f_flags = 0 };
		char ubuf[128];
		loff_t off = 0;
		ssize_t n;

		head = tail = 0;
		for (const char *p = "abc"; *p; p++)
			ring_push(*p);
		n = kbd_read(&f, ubuf, sizeof ubuf, &off);
		ok("drains the ring", n == 3 && !memcmp(ubuf, "abc", 3));

		head = tail = 0;
		f.f_flags = O_NONBLOCK;
		ok("empty + O_NONBLOCK -> -EAGAIN",
		   kbd_read(&f, ubuf, sizeof ubuf, &off) == -EAGAIN);

		f.f_flags = 0;
		head = tail = 0;
		for (int i = 0; i < BUF_SIZE - 1; i++)
			ring_push('z');
		n = kbd_read(&f, ubuf, sizeof ubuf, &off);
		ok("one read is capped at READ_MAX", n == READ_MAX);

		head = tail = 0;
		ring_push('q');
		ok("respects a short len", kbd_read(&f, ubuf, 1, &off) == 1);
	}

	puts("\npoll()");
	{
		struct file f = { .f_flags = 0 };
		poll_table pt;

		head = tail = 0;
		ok("empty ring -> no EPOLLIN", kbd_poll(&f, &pt) == 0);
		ring_push('a');
		ok("data ready -> EPOLLIN|EPOLLRDNORM",
		   kbd_poll(&f, &pt) == (EPOLLIN | EPOLLRDNORM));
	}

	puts("\ninit/exit smoke");
	ok("kbd_init() succeeds against stubs", kbd_init() == 0);
	kbd_exit();
	ok("file_operations wired up",
	   fops.read && fops.open && fops.release && fops.poll);

	printf("\n%s: %d failure(s)\n\n", fails ? "FAILED" : "all passed", fails);
	return fails != 0;
}
