#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "bignum.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 500

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static void fib_sequence(long long k, bignum *result)
{
    bignum fk0;   // F(k)
    bignum fk1;   // F(k+1)
    bignum fk20;  // F(2k)
    bignum fk21;  // F(2k+1)
    bignum tmp1, tmp2;

    if (k < 2) {
        bn_release(result);
        bn_init(result, k);
        return;
    }

    bn_init(&fk0, 1);
    bn_init(&fk1, 1);
    bn_init(&fk20, 0);
    bn_init(&fk21, 0);
    bn_init(&tmp1, 0);
    bn_init(&tmp2, 0);

    long long mask = 1 << (fls(k) - 2);

    while (mask) {
        bn_add(&fk1, &fk1, &tmp1);
        bn_sub(&tmp1, &fk0, &tmp1);
        bn_mul(&fk0, &tmp1, &fk20);  // fk20 = fk0 * (2 * fk1 - fk0)

        bn_mul(&fk0, &fk0, &tmp1);
        bn_mul(&fk1, &fk1, &tmp2);
        bn_add(&tmp1, &tmp2, &fk21);  // fk21 = fk0 * fk0 + fk1 * fk1

        bn_assign(&fk0, &fk20);  // fk0 = fk20
        bn_assign(&fk1, &fk21);  // fk1 = fk21

        if (k & mask) {
            bn_add(&fk0, &fk1, &fk20);  // fk20 = fk0 + fk1
            bn_assign(&fk0, &fk1);      // fk0 = fk1
            bn_assign(&fk1, &fk20);     // fk1 = fk20
        }

        mask >>= 1;
    }

    bn_assign(result, &fk0);  // return fk0

    bn_release(&fk0);
    bn_release(&fk1);
    bn_release(&fk20);
    bn_release(&fk21);
    bn_release(&tmp1);
    bn_release(&tmp2);
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

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    bignum result;
    int n;

    bn_init(&result, 0);

    fib_sequence(*offset, &result);

    n = bn_size(&result);

    if (n + 1 <= size) {
        if (copy_to_user(buf, bn_str(&result), n + 1))
            return -EFAULT;
    }

    bn_release(&result);

    return n;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
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
