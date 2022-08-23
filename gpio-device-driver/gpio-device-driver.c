#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define GPIO_60     (60)

dev_t dev = 0;
static struct class *dev_class;
static struct cdev foo_cdev;

static int __init foo_driver_init(void);
static void __exit foo_driver_exit(void);

/************************ Driver functions ************************/
static int foo_open(struct inode *inode , struct file *file);
static int foo_release(struct inode *inode , struct file *file);
static ssize_t foo_read(struct file *filp, char __user *buf, size_t len,
                loff_t *off);
static ssize_t foo_write(struct file *filp, const char *buf, size_t len,
                loff_t *off);
/******************************************************************/


//File operation structure
static struct file_operations fops =
{
    .owner      = THIS_MODULE,
    .read       = foo_read,
    .release    = foo_release,
    .open       = foo_open,
    .write      = foo_write,
};

/*
 * This function will be called when we open the Device file
 */
static int foo_open(struct inode *inode , struct file *file)
{
    pr_info("Device File Opened...!!!\n");
    return 0;
}

/*
 * This function will be called when we close the Device file
 */
static int foo_release(struct inode *inode , struct file *file)
{
    pr_info("Device File Closed...!!!\n");
    return 0;
}

/*
 * This function will be called when we close the Device file
 */
static ssize_t foo_read(struct file *filp,
                char __user *buf, size_t len, loff_t *off)
{
    uint8_t gpio_state = 0;

    // read GPIO value
    gpio_state = gpio_get_value(GPIO_60);

    // write to user
    len = 1;
    if(copy_to_user(buf, &gpio_state, len) > 0) {
        pr_err("ERROR: Not all bytes have been copied to user!\n");
    }

    pr_info("Read function: GPIO_60 = %d \n", gpio_state);

    return 0;
}

/*
 * This function will be called when we write the Device file
 */
static ssize_t foo_write(struct file *filp,
                const char __user *buf, size_t len, loff_t *off)
{
    uint8_t rec_buf[10] = {0};

    if(copy_from_user(rec_buf, buf, len) > 0) {
        pr_err("ERROR: Not all the bytes have been copied from user");
    }

    pr_info("Write Function: GPIO_60 Set = %c\n", rec_buf[0]);

    if(rec_buf[0] == '1') {
        // set the GPIO value to HIGH
        gpio_set_value(GPIO_60, 1);
    } else if (rec_buf[0] == '0') {
        // set the GPIO value to LOW
        gpio_set_value(GPIO_60, 0);
    } else {
        pr_err("Unknown command: Please set 1 or 0");
    }

    return len;
}

/*
 * Module Init
 */
 static int __init foo_driver_init(void)
{
    // Allocating Major number
    if((alloc_chrdev_region(&dev, 0, 1, "gpio_Dev")) < 0) {
        pr_err("Cannot allocate major number\n");
        goto r_unreg;
    }
    pr_info("Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));

    // Creating cdev structure
    cdev_init(&foo_cdev, &fops);

    // Adding character device to the system
    if((cdev_add(&foo_cdev, dev, 1)) < 0) {
        pr_err("Cannot add the device to the system\n");
        goto r_del;
    }

    // Creating struct class
    if((dev_class = class_create(THIS_MODULE, "gpio_Dev")) == NULL) {
        pr_err("Cannot create the struct class\n");
        goto r_class;
    }

    // Creating device
    if((device_create(dev_class, NULL, dev, NULL, "gpio_Dev")) == NULL) {
        pr_err("Cannot create the Device\n");
        goto r_device;
    }

    // Checking the GPIO is valid or not
    if(gpio_is_valid(GPIO_60) == false) {
        pr_err("GPIO %d is not valid\n", GPIO_60);
        goto r_device;
    }

    // Requesting the GPIO
    if(gpio_request(GPIO_60, "GPIO_60") < 0) {
        pr_err("ERROR: GPIO %d request\n", GPIO_60);
        goto r_gpio;
    }

    // Configure the GPIO as output
    gpio_direction_output(GPIO_60, 0);

    /* Using this call the GPIO 21 will be visible in /sys/class/gpio/
     * Now you can change the gpio values by using below commands also.
     * echo 1 > /sys/class/gpio/gpio60/value  (turn ON the LED)
     * echo 0 > /sys/class/gpio/gpio60/value  (turn OFF the LED)
     * cat /sys/class/gpio/gpio60/value  (read the value LED)
     *
     * the second argument prevents the direction from being changed.
     */
    gpio_export(GPIO_60, false);

    pr_info("Device Driver Insert... Done!!!\n");
    return 0;

r_gpio:
    gpio_free(GPIO_60);
r_device:
    device_destroy(dev_class, dev);
r_class:
    class_destroy(dev_class);
r_del:
    cdev_del(&foo_cdev);
r_unreg:
    unregister_chrdev_region(dev, 1);

    return -1;
}

/*
 * Module exit
 */
static void __exit foo_driver_exit(void)
{
    gpio_unexport(GPIO_60);
    gpio_free(GPIO_60);
    device_destroy(dev_class,dev);
    class_destroy(dev_class);
    cdev_del(&foo_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("Device Driver Exit...\n");
}

module_init(foo_driver_init);
module_exit(foo_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rafaeluan@yahoo.com.br");
MODULE_DESCRIPTION("A simple gpio device driver, using GPIO legacy api");
MODULE_VERSION("0.1");
