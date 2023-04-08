#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "bn_kernel.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 100

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ktime_t kt;

/*
static long long fib_sequence_dp(long long k)
{
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}
static __uint128_t fib_sequence_fast_doubling(int k)
{
    if (k < 2)
        return k;
    __uint128_t a = 0, b = 1, t1, t2;
    int len = 32 - __builtin_clz(k);
    for (; len > 0; len--) {
        t1 = a * (2 * b - a);
        t2 = b * b + a * a;
        a = t1;
        b = t2;
        if ((k >> (len - 1)) & 0x1) {
            t1 = a + b;
            a = b;
            b = t1;
        }
    }
    return a;
}
*/
void fib_fdoubling_bn(bn *dest, unsigned int n)
{
    bn_resize(dest, 1);
    if (n < 2) {  // Fib(0) = 0, Fib(1) = 1
        dest->number[0] = n;
        return;
    }

    bn *f1 = dest;        /* F(k) */
    bn *f2 = bn_alloc(1); /* F(k+1) */
    f1->number[0] = 0;
    f2->number[0] = 1;
    bn *k1 = bn_alloc(1);
    bn *k2 = bn_alloc(1);

    /* walk through the digit of n */
    for (unsigned int i = 1U << 31; i; i >>= 1) {
        /* F(2k) = F(k) * [ 2 * F(k+1) – F(k) ] */
        bn_cpy(k1, f2);
        bn_lshift(k1, 1);
        bn_sub(k1, f1, k1);
        bn_mult(k1, f1, k1);
        /* F(2k+1) = F(k)^2 + F(k+1)^2 */
        bn_mult(f1, f1, f1);
        bn_mult(f2, f2, f2);
        bn_cpy(k2, f1);
        bn_add(k2, f2, k2);
        if (n & i) {
            bn_cpy(f1, k2);
            bn_cpy(f2, k1);
            bn_add(f2, k2, f2);
        } else {
            bn_cpy(f1, k1);
            bn_cpy(f2, k2);
        }
    }
    // return f[0]
    bn_free(f2);
    bn_free(k1);
    bn_free(k2);
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

bn *fib_time_proxy(long long k)
{
    kt = ktime_get();
    bn *dest = bn_alloc(1);
    fib_fdoubling_bn(dest, k);
    kt = ktime_sub(ktime_get(), kt);
    return dest;
}

static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    bn *dest = fib_time_proxy(*offset);
    // char *result = bn_to_string(dest);
    // int len = strlen(result);
    int len = dest->size;
    copy_to_user(buf, (char *) dest->number, 4 * len);
    return (ssize_t) len;
}

static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
