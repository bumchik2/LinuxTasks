#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>

#define MAX_DEV 1

static int mychardev_open(struct inode *inode, struct file *file);
static int mychardev_release(struct inode *inode, struct file *file);
static long mychardev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t mychardev_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t mychardev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

static const struct file_operations mychardev_fops = {
    .owner      = THIS_MODULE,
    .open       = mychardev_open,
    .release    = mychardev_release,
    .unlocked_ioctl = mychardev_ioctl,
    .read       = mychardev_read,
    .write       = mychardev_write
};

struct mychar_device_data {
    struct cdev cdev;
};

static int dev_major = 0;
static struct class *mychardev_class = NULL;
static struct mychar_device_data mychardev_data[MAX_DEV];

const size_t BUF_SIZE = 10 * PAGE_SIZE;
static char* in_buf;
static char* out_buf;

char* in_buf_start = NULL;
char* in_buf_end = NULL;
char* out_buf_start = NULL;
char* out_buf_end = NULL;

const size_t max_surname_size = 30;
const size_t max_phone_size = 30;
struct user_info {
    char* surname;
    char* phone;
    struct user_info* next;
};

struct user_info* first = NULL;

struct user_info* create_user_info(char* surname, char* phone) {
    int i;    
    struct user_info* new_user_info = kzalloc(sizeof(struct user_info), GFP_KERNEL);
    new_user_info->surname = kzalloc(max_surname_size, GFP_KERNEL);
    new_user_info->phone = kzalloc(max_phone_size, GFP_KERNEL);
    new_user_info->next = NULL;
    for (i = 0; i < strlen(surname); ++i) {
        new_user_info->surname[i] = surname[i];
    }
    new_user_info->surname[strlen(surname)] = 0;
    for (i = 0; i < strlen(phone); ++i) {
        new_user_info->phone[i] = phone[i];
    }
    new_user_info->phone[strlen(phone)] = 0;
    return new_user_info;
}

void free_user_info(struct user_info* user_info_to_free) {
    kfree(user_info_to_free->surname);
    kfree(user_info_to_free->phone);
    kfree(user_info_to_free);
}

struct user_info* get_last(void) {
    if (first == NULL) {
        return NULL;
    }
    struct user_info* tmp = first;
    while (tmp->next != NULL) {
        tmp = tmp->next;
    }
    return tmp;
}

void add_new_user_info(struct user_info* new_user_info) {
    if (first == NULL) {
        first = new_user_info;
        return;
    }
    struct user_info* last = get_last();
    last->next = new_user_info;
}

void remove_all_user_info_after(struct user_info* user_info_to_remove) {
    if (user_info_to_remove == NULL) {
        return;
    }
    remove_all_user_info_after(user_info_to_remove->next);
    free_user_info(user_info_to_remove);
}

void remove_all_user_info(void) {
    if (first == NULL) {
        return;
    }
    remove_all_user_info_after(first);
    first = NULL;
}

static int mychardev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int __init mychardev_init(void)
{
    printk(KERN_INFO "Hello, loading\n");
    printk(KERN_INFO "BUF_SIZE: %ld", BUF_SIZE);

    int err;
    int i;

    dev_t dev;

    err = alloc_chrdev_region(&dev, 0, MAX_DEV, "mychardev");

    dev_major = MAJOR(dev);

    mychardev_class = class_create(THIS_MODULE, "mychardev");
    mychardev_class->dev_uevent = mychardev_uevent;

    for (i = 0; i < MAX_DEV; i++) {
        cdev_init(&mychardev_data[i].cdev, &mychardev_fops);
        mychardev_data[i].cdev.owner = THIS_MODULE;

        cdev_add(&mychardev_data[i].cdev, MKDEV(dev_major, i), 1);

        device_create(mychardev_class, NULL, MKDEV(dev_major, i), NULL, "mychardev-%d", i);
    }

    in_buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    in_buf_start = in_buf;
    in_buf_end = in_buf_end;
    out_buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    out_buf_start = out_buf;
    out_buf_end = out_buf_end;

    return 0;
}

static void __exit mychardev_exit(void)
{
    int i;

    for (i = 0; i < MAX_DEV; i++) {
        device_destroy(mychardev_class, MKDEV(dev_major, i));
    }

    class_unregister(mychardev_class);
    class_destroy(mychardev_class);

    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

    if (in_buf) {
        kfree(in_buf);
        in_buf = NULL;
    }
    if (out_buf) {
        kfree(out_buf);
        out_buf = NULL;
    }
    remove_all_user_info();

    printk(KERN_INFO "Leaving\n");
}

static int mychardev_open(struct inode *inode, struct file *file)
{
    printk("MYCHARDEV: Device open\n");
    return 0;
}

static int mychardev_release(struct inode *inode, struct file *file)
{
    printk("MYCHARDEV: Device close\n");
    return 0;
}

static long mychardev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("MYCHARDEV: Device ioctl\n");
    return 0;
}

static ssize_t mychardev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    // read data from out_buf
    if (out_buf_end - out_buf_start < count) {
        count = out_buf_end - out_buf_start;
    }
    int nbytes = count - copy_to_user(buf, out_buf + *offset, count);
    *offset += nbytes;
    out_buf_start += nbytes;
    printk(KERN_INFO "Read nbytes = %d, offset = %d\n", nbytes, (int)*offset);
    // possibly I should do something like this?
    // *offset = 0;
    // out_buf_start = out_buf;
    // out_buf_end = out_buf;
    return nbytes;
}

void get_user_info(const char* surname) {
    // TODO
}

void remove_user_info(const char* surname) {
    // TODO
}

void process_in_buf(void) {
    // process data from in_buf to update out_buf
    // a <surname> <phone>
    // g <surname>
    // r <surname>
    char* surname;
    char* phone;
    char first_char;
    while(in_buf_start != NULL) {
        first_char = in_buf_start[0];
        strsep(&in_buf_start, " ");
        if (first_char == 'a') { // add
            surname = in_buf_start;
            strsep(&in_buf_start, " ");
            phone = in_buf_start;
            add_new_user_info(create_user_info(surname, phone));
        } else if (first_char == 'g') { // get
            surname = in_buf_start;
            get_user_info(surname);
        } else if (first_char == 'r') { // remove
            surname = in_buf_start;
            remove_user_info(surname);
        }
        strsep(&in_buf_start, " ");
    }
    in_buf_start = in_buf_end;
    // possibly I should do something like this?
    // *offset = 0;
    // in_buf_start = in_buf;
    // in_buf_end = in_buf;
}

static ssize_t mychardev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    // write data into in_buf
    int nbytes = count - copy_from_user(in_buf + *offset, buf, count);
    *offset += nbytes;
    in_buf_end = in_buf_start + nbytes;
    printk(KERN_INFO "write nbytes = %d, offset = %d\n", nbytes, (int)*offset);
    process_in_buf();
    return nbytes;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bumchik2");

module_init(mychardev_init);
module_exit(mychardev_exit);
