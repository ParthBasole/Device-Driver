// ps2kbd - PS/2 keyboard char driver. Claims IRQ 1 and the i8042 data port,
// decodes scancode set 1 (US layout) and serves the characters via /dev/ps2kbd.
//
// The built-in i8042 driver owns those resources, so request_irq() returns
// -EBUSY until you `make unbind`. That kills your PS/2 keyboard until
// `make rebind`, so do it in a VM or over SSH.

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/ctype.h>
#include <linux/version.h>

#define DEVICE_NAME "ps2kbd"
#define KBD_IRQ     1
#define KBD_DATA    0x60	// i8042 data port
#define KBD_STATUS  0x64	// i8042 status port
#define BUF_SIZE    256		// ring capacity, power of two
#define READ_MAX    64		// cap on one read(), bounds the stack copy

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Parth Basole");
MODULE_DESCRIPTION("PS/2 keyboard driver: decodes scancodes from port 0x60 on IRQ 1");
MODULE_VERSION("1.0");

// US layout, scancode set 1 make codes: [0] plain, [1] shifted.
static const char kbd_map[2][128] = {
	{
		[0x02] = '1',  [0x03] = '2', [0x04] = '3',  [0x05] = '4',
		[0x06] = '5',  [0x07] = '6', [0x08] = '7',  [0x09] = '8',
		[0x0a] = '9',  [0x0b] = '0', [0x0c] = '-',  [0x0d] = '=',
		[0x0e] = '\b', [0x0f] = '\t',
		[0x10] = 'q',  [0x11] = 'w', [0x12] = 'e',  [0x13] = 'r',
		[0x14] = 't',  [0x15] = 'y', [0x16] = 'u',  [0x17] = 'i',
		[0x18] = 'o',  [0x19] = 'p', [0x1a] = '[',  [0x1b] = ']',
		[0x1c] = '\n',
		[0x1e] = 'a',  [0x1f] = 's', [0x20] = 'd',  [0x21] = 'f',
		[0x22] = 'g',  [0x23] = 'h', [0x24] = 'j',  [0x25] = 'k',
		[0x26] = 'l',  [0x27] = ';', [0x28] = '\'', [0x29] = '`',
		[0x2b] = '\\',
		[0x2c] = 'z',  [0x2d] = 'x', [0x2e] = 'c',  [0x2f] = 'v',
		[0x30] = 'b',  [0x31] = 'n', [0x32] = 'm',  [0x33] = ',',
		[0x34] = '.',  [0x35] = '/', [0x37] = '*',  [0x39] = ' ',
	},
	{
		[0x02] = '!',  [0x03] = '@', [0x04] = '#',  [0x05] = '$',
		[0x06] = '%',  [0x07] = '^', [0x08] = '&',  [0x09] = '*',
		[0x0a] = '(',  [0x0b] = ')', [0x0c] = '_',  [0x0d] = '+',
		[0x0e] = '\b', [0x0f] = '\t',
		[0x10] = 'Q',  [0x11] = 'W', [0x12] = 'E',  [0x13] = 'R',
		[0x14] = 'T',  [0x15] = 'Y', [0x16] = 'U',  [0x17] = 'I',
		[0x18] = 'O',  [0x19] = 'P', [0x1a] = '{',  [0x1b] = '}',
		[0x1c] = '\n',
		[0x1e] = 'A',  [0x1f] = 'S', [0x20] = 'D',  [0x21] = 'F',
		[0x22] = 'G',  [0x23] = 'H', [0x24] = 'J',  [0x25] = 'K',
		[0x26] = 'L',  [0x27] = ':', [0x28] = '"',  [0x29] = '~',
		[0x2b] = '|',
		[0x2c] = 'Z',  [0x2d] = 'X', [0x2e] = 'C',  [0x2f] = 'V',
		[0x30] = 'B',  [0x31] = 'N', [0x32] = 'M',  [0x33] = '<',
		[0x34] = '>',  [0x35] = '?', [0x37] = '*',  [0x39] = ' ',
	},
};

// Numeric keypad. Kept out of kbd_map because these must not follow shift --
// keypad 7 is '7', never '&'. Digits and '.' are Num Lock functions; '-' and
// '+' work either way, as does keypad '*' (0x37, outside this range, so it
// stays in kbd_map above).
#define KP_FIRST 0x47
#define KP_LAST  0x53
static const char kbd_keypad[KP_LAST - KP_FIRST + 1] = {
	[0x47 - KP_FIRST] = '7', [0x48 - KP_FIRST] = '8', [0x49 - KP_FIRST] = '9',
	[0x4a - KP_FIRST] = '-',
	[0x4b - KP_FIRST] = '4', [0x4c - KP_FIRST] = '5', [0x4d - KP_FIRST] = '6',
	[0x4e - KP_FIRST] = '+',
	[0x4f - KP_FIRST] = '1', [0x50 - KP_FIRST] = '2', [0x51 - KP_FIRST] = '3',
	[0x52 - KP_FIRST] = '0', [0x53 - KP_FIRST] = '.',
};

static int major;
static struct class *kbd_class;
static atomic_t opens = ATOMIC_INIT(0);
static bool got_data, got_status;	// which ports we actually reserved

// Ring buffer: produced in hard IRQ, consumed by read().
static char ring[BUF_SIZE];
static unsigned int head, tail;
static DEFINE_SPINLOCK(ring_lock);
// Named kbd_waitq, not readq -- <linux/io.h> defines readq() as an MMIO accessor.
static DECLARE_WAIT_QUEUE_HEAD(kbd_waitq);

static bool ring_empty(void)
{
	return READ_ONCE(head) == READ_ONCE(tail);
}

static void ring_push(char c)
{
	unsigned int next;
	unsigned long flags;

	spin_lock_irqsave(&ring_lock, flags);
	next = (head + 1) & (BUF_SIZE - 1);
	if (next != tail) {		// full: drop, the reader is too slow
		ring[head] = c;
		head = next;
	}
	spin_unlock_irqrestore(&ring_lock, flags);

	wake_up_interruptible(&kbd_waitq);
}

// Decode one scancode. Multi-byte sequences (0xe0 extended, 0xe1 Pause) are
// tracked across calls so their payload is never mistaken for a key -- that
// matters because arrows and PrintScreen emit a fake shift (0xe0 0x2a) which
// would otherwise invert our shift state for every subsequent keystroke.
static void kbd_decode(u8 sc)
{
	static bool shift, caps, ext;
	static bool num = true;			// most BIOSes enable Num Lock at boot
	static int pause_left;
	unsigned char code = sc & 0x7f;
	bool release = sc & 0x80;
	char c;

	if (sc == 0xe1) {			// Pause: e1 1d 45 e1 9d c5
		pause_left = 2;
		return;
	}
	if (pause_left) {
		pause_left--;
		return;
	}
	if (sc == 0xe0) {
		ext = true;
		return;
	}
	if (ext) {				// extended: keypad, arrows, r-ctrl/alt
		ext = false;
		if (!release && code == 0x1c)
			ring_push('\n');	// keypad Enter
		else if (!release && code == 0x35)
			ring_push('/');		// keypad /
		return;
	}

	if (code == 0x2a || code == 0x36) {	// either shift
		shift = !release;
		return;
	}
	if (code == 0x3a) {			// caps lock, toggle on press edge
		caps ^= !release;
		return;
	}
	if (code == 0x45) {			// num lock, toggle on press edge
		num ^= !release;
		return;
	}
	if (release)
		return;

	if (code >= KP_FIRST && code <= KP_LAST) {
		c = kbd_keypad[code - KP_FIRST];
		// Without Num Lock these keys are Home/arrows/PgUp/Del, which we
		// have no character for -- drop them rather than emit a digit.
		if (c && (num || c == '-' || c == '+'))
			ring_push(c);
		return;
	}

	c = kbd_map[shift][code];
	if (!c)
		return;
	if (caps && isalpha(c))
		c ^= 0x20;			// caps inverts case, relative to shift
	ring_push(c);
}

static irqreturn_t kbd_isr(int irq, void *dev_id)
{
	u8 status = inb(KBD_STATUS);
	u8 sc;

	if (!(status & 0x01))		// output buffer empty: not our interrupt
		return IRQ_NONE;

	sc = inb(KBD_DATA);		// always drain, or the line keeps re-asserting
	if (!(status & 0x20))		// bit 5 set => byte came from the aux/mouse port
		kbd_decode(sc);

	return IRQ_HANDLED;
}

static int kbd_open(struct inode *inode, struct file *filp)
{
	pr_info("opened (%d)\n", atomic_inc_return(&opens));
	return 0;
}

static int kbd_release(struct inode *inode, struct file *filp)
{
	pr_info("closed\n");
	return 0;
}

static ssize_t kbd_read(struct file *filp, char __user *ubuf, size_t len,
			loff_t *off)
{
	char tmp[READ_MAX];
	unsigned long flags;
	size_t n = 0;

	if (!len)
		return 0;

	for (;;) {
		spin_lock_irqsave(&ring_lock, flags);
		while (n < len && n < sizeof(tmp) && head != tail) {
			tmp[n++] = ring[tail];
			tail = (tail + 1) & (BUF_SIZE - 1);
		}
		spin_unlock_irqrestore(&ring_lock, flags);

		if (n)
			break;
		// A concurrent reader may have drained the ring before we took
		// the lock. Loop instead of returning 0 -- that reads as EOF.
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(kbd_waitq, !ring_empty()))
			return -ERESTARTSYS;
	}

	if (copy_to_user(ubuf, tmp, n))
		return -EFAULT;

	pr_debug("sent %zu char(s)\n", n);
	return n;
}

static __poll_t kbd_poll(struct file *filp, poll_table *pt)
{
	poll_wait(filp, &kbd_waitq, pt);
	return ring_empty() ? 0 : EPOLLIN | EPOLLRDNORM;
}

static const struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = kbd_open,
	.read    = kbd_read,
	.poll    = kbd_poll,
	.release = kbd_release,
};

// Undo everything init() set up. Only release ports we actually got.
static void kbd_teardown(void)
{
	if (got_status)
		release_region(KBD_STATUS, 1);
	if (got_data)
		release_region(KBD_DATA, 1);
	device_destroy(kbd_class, MKDEV(major, 0));
	class_destroy(kbd_class);	// also unregisters the class
	unregister_chrdev(major, DEVICE_NAME);
}

static int __init kbd_init(void)
{
	struct device *dev;
	int ret;

	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0)
		return major;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	kbd_class = class_create(DEVICE_NAME);
#else
	kbd_class = class_create(THIS_MODULE, DEVICE_NAME);
#endif
	if (IS_ERR(kbd_class)) {
		unregister_chrdev(major, DEVICE_NAME);
		return PTR_ERR(kbd_class);
	}

	dev = device_create(kbd_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
	if (IS_ERR(dev)) {
		class_destroy(kbd_class);
		unregister_chrdev(major, DEVICE_NAME);
		return PTR_ERR(dev);
	}

	// Bookkeeping only -- inb() works either way.
	got_data   = request_region(KBD_DATA, 1, DEVICE_NAME) != NULL;
	got_status = request_region(KBD_STATUS, 1, DEVICE_NAME) != NULL;
	if (!got_data || !got_status)
		pr_warn("could not reserve ports 0x%x/0x%x\n", KBD_DATA, KBD_STATUS);

	ret = request_irq(KBD_IRQ, kbd_isr, 0, DEVICE_NAME, &kbd_isr);
	if (ret) {
		pr_alert("no IRQ %d (%d) -- unbind i8042 first: `make unbind`\n",
			 KBD_IRQ, ret);
		kbd_teardown();
		return ret;
	}

	pr_info("ready on /dev/%s (major %d, IRQ %d)\n", DEVICE_NAME, major, KBD_IRQ);
	return 0;
}

static void __exit kbd_exit(void)
{
	free_irq(KBD_IRQ, &kbd_isr);
	kbd_teardown();
	pr_info("unloaded -- run `make rebind` to restore i8042\n");
}

module_init(kbd_init);
module_exit(kbd_exit);
