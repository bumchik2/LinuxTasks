#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>

#define MAX_DEV 1

static int fifo_open(struct inode *inode, struct file *file);
static int fifo_release(struct inode *inode, struct file *file);
static long fifo_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t fifo_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t fifo_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

static const struct file_operations fifo_fops = {
    .owner      = THIS_MODULE,
    .open       = fifo_open,
    .release    = fifo_release,
    .unlocked_ioctl = fifo_ioctl,
    .read       = fifo_read,
    .write       = fifo_write
};

struct mychar_device_data {
    struct cdev cdev;
};

static int dev_major = 0;
static struct class *fifo_class = NULL;
static struct mychar_device_data fifo_data[MAX_DEV];

const size_t BUF_SIZE = 10 * PAGE_SIZE;
static char* in_buf;

char* in_buf_start = NULL;
char* in_buf_end = NULL;

const size_t max_buffer_size = 256;
struct fifo_node {
    char* buffer;
    struct fifo_node* next;
};

struct fifo_node* first = NULL;

struct fifo_node* create_fifo_node(char* buffer) {
    int i;
    struct fifo_node* new_fifo_node = kzalloc(sizeof(struct fifo_node), GFP_KERNEL);
    new_fifo_node->buffer = kzalloc(max_buffer_size, GFP_KERNEL);
    new_fifo_node->next = NULL;
    for (i = 0; i < strlen(buffer); ++i) {
        new_fifo_node->buffer[i] = buffer[i];
    }
    new_fifo_node->buffer[strlen(buffer)] = '\0';
    printk(KERN_INFO "new buffer length: %ld", strlen(buffer));
    return new_fifo_node;
}

void free_fifo_node(struct fifo_node* fifo_node_to_free) {
    kfree(fifo_node_to_free->buffer);
    kfree(fifo_node_to_free);
}

struct fifo_node* get_last(void) {
    if (first == NULL) {
        return NULL;
    }
    struct fifo_node* tmp = first;
    while (tmp->next != NULL) {
        tmp = tmp->next;
    }
    return tmp;
}

void add_new_fifo_node(struct fifo_node* new_fifo_node) {
    if (first == NULL) {
        first = new_fifo_node;
        return;
    }
    struct fifo_node* last = get_last();
    last->next = new_fifo_node;
}

void remove_all_fifo_node_after(struct fifo_node* fifo_node_to_remove) {
    if (fifo_node_to_remove == NULL) {
        return;
    }
    remove_all_fifo_node_after(fifo_node_to_remove->next);
    free_fifo_node(fifo_node_to_remove);
}

void remove_all_fifo_node(void) {
    if (first == NULL) {
        return;
    }
    remove_all_fifo_node_after(first);
    first = NULL;
}

static int fifo_uevent(struct device *dev, struct kobj_uevent_env *env) {
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int __init fifo_init(void) {
    printk(KERN_INFO "Hello, loading kernel\n");
    printk(KERN_INFO "BUF_SIZE: %ld", BUF_SIZE);

    int err;
    int i;

    dev_t dev;
    err = alloc_chrdev_region(&dev, 0, MAX_DEV, "fifo");

    dev_major = MAJOR(dev);

    fifo_class = class_create(THIS_MODULE, "fifo");
    fifo_class->dev_uevent = fifo_uevent;

    for (i = 0; i < MAX_DEV; i++) {
        cdev_init(&fifo_data[i].cdev, &fifo_fops);
        fifo_data[i].cdev.owner = THIS_MODULE;
        cdev_add(&fifo_data[i].cdev, MKDEV(dev_major, i), 1);
        device_create(fifo_class, NULL, MKDEV(dev_major, i), NULL, "fifo-%d", i);
    }

    in_buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    in_buf_start = in_buf;
    in_buf_end = in_buf;
    return 0;
}

static void __exit fifo_exit(void)
{
    int i;

    for (i = 0; i < MAX_DEV; i++) {
        device_destroy(fifo_class, MKDEV(dev_major, i));
    }

    class_unregister(fifo_class);
    class_destroy(fifo_class);

    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

    if (in_buf) {
        kfree(in_buf);
        in_buf = NULL;
    }
    remove_all_fifo_node();

    printk(KERN_INFO "Leaving\n");
}

static int fifo_open(struct inode *inode, struct file *file)
{
    printk("FIFO: Device open\n");
    return 0;
}

static int fifo_release(struct inode *inode, struct file *file)
{
    printk("FIFO: Device close\n");
    return 0;
}

static long fifo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("FIFO: Device ioctl\n");
    return 0;
}

void remove_last_node(void) {
    if (first == NULL) {
        printk(KERN_INFO "trying to remove last node from empty fifo! it's a bug!");
        return;
    }
    if (first->next == NULL) {
        free_fifo_node(first);
        first = NULL;
        return;
    }
    struct fifo_node* last = get_last();
    struct fifo_node* tmp = first;
    while(tmp->next != last) {
        tmp = tmp->next;
    }
    tmp->next = NULL;
    free_fifo_node(last);
}

bool already_read = false;

static ssize_t fifo_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    if (already_read) {
        already_read = false;
        return 0; // read should not be called again
    }
    already_read = true;
    if (first == NULL) {
        printk(KERN_INFO "fifo is empty!");
        return 0;
    }
    printk(KERN_INFO "read");
    struct fifo_node* last = get_last();  
    char* buffer = last->buffer;
    if (strlen(buffer) < count) {
        count = strlen(buffer);
    }
    int nbytes = count - copy_to_user(buf, buffer, count);
    printk(KERN_INFO "Read nbytes = %d, offset = %d\n", nbytes, (int)*offset);
    remove_last_node();
    return nbytes;
}

void process_in_buf(void) {
    char* buffer;
    buffer = in_buf_start;
    buffer[strcspn(buffer, "\r\n")] = 0;
    printk(KERN_INFO "adding buffer %s", buffer);
    add_new_fifo_node(create_fifo_node(buffer));
}

static ssize_t fifo_write(struct file *file, const char __user *buf, size_t count, loff_t *offset) {
    int nbytes = count - copy_from_user(in_buf + *offset, buf, count);
    *offset += nbytes;
    printk(KERN_INFO "write nbytes = %d, offset = %d\n", nbytes, (int)*offset);

    process_in_buf();

    *offset = 0;
    int i;
    for (i = 0; i < BUF_SIZE; ++i) {
        in_buf[i] = 0;
    }
    in_buf_start = in_buf;
    in_buf_end = in_buf;
    return nbytes;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bumchik2");

module_init(fifo_init);
module_exit(fifo_exit);




