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
#include <linux/interrupt.h>
#include <linux/jiffies.h>

#define GPIO_48_IN      (48)
#define GPIO_60_OUT     (60)

dev_t dev = 0;
static struct class *dev_class;
static struct cdev foo_cdev;

extern unsigned long volatile jiffies;
unsigned long old_jiffie = 0;

// Toggle value in GPIO_48_IN
unsigned int led_toggle = 0;

// This is used for storing the IRQ number for the GPIO
unsigned int GPIO_irqNumber;

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
    gpio_state = gpio_get_value(GPIO_60_OUT);

    // write to user
    len = 1;
    if(copy_to_user(buf, &gpio_state, len) > 0) {
        pr_err("ERROR: Not all bytes have been copied to user!\n");
    }

    pr_info("Read function: GPIO_60_OUT = %d \n", gpio_state);

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

    pr_info("Write Function: GPIO_60_OUT Set = %c\n", rec_buf[0]);

    if(rec_buf[0] == '1') {
        // set the GPIO value to HIGH
        gpio_set_value(GPIO_60_OUT, 1);
    } else if (rec_buf[0] == '0') {
        // set the GPIO value to LOW
        gpio_set_value(GPIO_60_OUT, 0);
    } else {
        pr_err("Unknown command: Please set 1 or 0");
    }

    return len;
}

/*
 * Interrupt handler for GPIO 25. This will be called whenever 
 * there is a raising edge detected.
 */ 
static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
    static unsigned long flags = 0;

    unsigned long diff = jiffies - old_jiffie;
    if(diff < 20)
    {
        return IRQ_HANDLED;
    }

    old_jiffie = jiffies;

    local_irq_save(flags);
    led_toggle = (0x01 ^ led_toggle);
    gpio_set_value(GPIO_60_OUT, led_toggle);
    pr_info("Interrupt Ocurred: GPIO_60_OUT : %d", gpio_get_value(GPIO_60_OUT));
    local_irq_restore(flags);
    return IRQ_HANDLED;
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
    if(gpio_is_valid(GPIO_60_OUT) == false) {
        pr_err("GPIO %d is not valid\n", GPIO_60_OUT);
        goto r_device;
    }

    // Requesting the GPIO
    if(gpio_request(GPIO_60_OUT, "GPIO_60_OUT") < 0) {
        pr_err("ERROR: GPIO %d request\n", GPIO_60_OUT);
        goto r_gpio_out;
    }

    // Configure the GPIO as output
    gpio_direction_output(GPIO_60_OUT, 0);

    // Input GPIO configuration 
    // Checking the GPIO is valid or not
    if(gpio_is_valid(GPIO_48_IN) == false) {
        pr_err("GPIO %d is not valid\n",GPIO_48_IN);
        goto r_gpio_in;
    }

    // Requesting the GPIO
    if(gpio_request(GPIO_48_IN, "GPIO_48_IN") < 0) {
        pr_err("ERROR: GPIO %d request\n", GPIO_48_IN);
        goto r_gpio_in;
    }

    // Configure the GPIO as input
    gpio_direction_input(GPIO_48_IN);

    // Debounce the button with a delay of 200ms
    if(gpio_set_debounce(GPIO_48_IN, 200) < 0) {
        pr_err("ERROR: gpio_set_debounce - %d\n", GPIO_48_IN);
    }

    // Get the IRQ number for our GPIO
    GPIO_irqNumber = gpio_to_irq(GPIO_48_IN);
    pr_info("GPIO_irqNumber = %d\n", GPIO_irqNumber);

    if(request_irq(GPIO_irqNumber,              // IRQ number
                    (void *)gpio_irq_handler,   // IRQ handler
                    IRQF_TRIGGER_RISING,        // Handler will be called in raising edge
                    "foo_device",               // Used to identify the device name using thos IRQ
                    NULL)) {                    // Device id for shared IRQ
        pr_err("Device cannot register IRQ");
        goto r_gpio_in;
    }

    pr_info("Device Driver Insert... Done!!!\n");
    return 0;

r_gpio_in:
    gpio_free(GPIO_48_IN);
r_gpio_out:
    gpio_free(GPIO_60_OUT);
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
    free_irq(GPIO_irqNumber, NULL);
    gpio_free(GPIO_48_IN);
    gpio_free(GPIO_60_OUT);
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
