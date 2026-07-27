#ifndef KSTUB_H
#define KSTUB_H
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>

#define KBUILD_MODNAME "ps2kbd"
#define MODULE_LICENSE(x)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_VERSION(x)
#define THIS_MODULE 0
#define __init
#define __exit
#define __user
#define module_init(x)
#define module_exit(x)
#define LINUX_VERSION_CODE 1
#define KERNEL_VERSION(a,b,c) 0

typedef unsigned char u8;
typedef int irqreturn_t;
typedef unsigned __poll_t;
typedef long long loff_t;
#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define EPOLLIN 1
#define EPOLLRDNORM 2
#define O_NONBLOCK 04000
#define EFAULT 14
#define EAGAIN 11
#define ERESTARTSYS 512

struct class; struct device; struct inode; struct module;
struct file { unsigned f_flags; };
typedef struct { int x; } poll_table;
typedef struct { int v; } atomic_t;
#define ATOMIC_INIT(n) { n }
static inline int atomic_inc_return(atomic_t *a) { return ++a->v; }

#define DEFINE_SPINLOCK(n) int n
#define spin_lock_irqsave(l, f)   do { (void)(l); (f) = 0; } while (0)
#define spin_unlock_irqrestore(l, f) do { (void)(l); (void)(f); } while (0)
#define DECLARE_WAIT_QUEUE_HEAD(n) int n
#define wake_up_interruptible(q)  do { (void)(q); } while (0)
#define wait_event_interruptible(q, c) ({ (void)(q); (c) ? 0 : -ERESTARTSYS; })
#define poll_wait(f, q, p) do { (void)(f); (void)(q); (void)(p); } while (0)
#define READ_ONCE(x) (x)

#define pr_info(...)  do { } while (0)
#define pr_warn(...)  do { } while (0)
#define pr_alert(...) do { } while (0)
#define pr_debug(...) do { } while (0)

static inline u8 inb(unsigned p) { (void)p; return 0; }
static inline unsigned long copy_to_user(void *d, const void *s, unsigned long n)
{ memcpy(d, s, n); return 0; }

struct file_operations { int owner; void *open, *read, *poll, *release; };
static inline int register_chrdev(int a, const char *b, const void *c)
{ (void)a;(void)b;(void)c; return 42; }
static inline void unregister_chrdev(int a, const char *b) { (void)a;(void)b; }
static inline struct class *class_create(const char *n) { (void)n; return (struct class *)1; }
static inline void class_destroy(struct class *c) { (void)c; }
static inline struct device *device_create(struct class *c, void *p, int d, void *v, const char *n)
{ (void)c;(void)p;(void)d;(void)v;(void)n; return (struct device *)1; }
static inline void device_destroy(struct class *c, int d) { (void)c;(void)d; }
#define MKDEV(a,b) 0
/* Real semantics: the kernel encodes errors as pointers in the last page. */
#define IS_ERR(p)  ((unsigned long)(void *)(p) >= (unsigned long)-4095)
#define PTR_ERR(p) ((long)(p))
static inline void *request_region(unsigned a, unsigned b, const char *c)
{ (void)a;(void)b;(void)c; return (void *)1; }
static inline void release_region(unsigned a, unsigned b) { (void)a;(void)b; }
static inline int request_irq(unsigned i, void *h, unsigned f, const char *n, void *d)
{ (void)i;(void)h;(void)f;(void)n;(void)d; return 0; }
static inline void free_irq(unsigned i, void *d) { (void)i;(void)d; }
#endif
