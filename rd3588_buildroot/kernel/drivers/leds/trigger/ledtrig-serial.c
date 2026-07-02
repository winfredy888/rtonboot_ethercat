#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/string.h>

static struct delayed_work led_work;
static struct led_classdev *serial_led_cdev;

static unsigned long blink_delay_on = 50;  // LED 亮起时间，单位为毫秒
static unsigned long blink_delay_off = 50; // LED 熄灭时间，单位为毫秒

static char serial_port_name[20] = {0};

static spinlock_t lock;

static void trigger_led_blink(struct work_struct *work)
{
    if (serial_led_cdev)
        led_blink_set_oneshot(serial_led_cdev, &blink_delay_on, &blink_delay_off, 0);
}

static ssize_t device_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t len;

    spin_lock_bh(&lock);
    len = sprintf(buf, "%s\n", serial_port_name);
    spin_unlock_bh(&lock);

    return len;
}

static ssize_t device_name_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    size_t len = min(size, sizeof(serial_port_name) - 1);

    spin_lock_bh(&lock);
    strncpy(serial_port_name, buf, len);
    serial_port_name[len] = '\0'; 
    spin_unlock_bh(&lock);

    return size;
}

static DEVICE_ATTR_RW(device_name);

static struct attribute *serial_trig_attrs[] = {
    &dev_attr_device_name.attr,
    NULL
};
ATTRIBUTE_GROUPS(serial_trig);

static int serial_trig_activate(struct led_classdev *led_cdev)
{
    int ret;
    struct device_node *node;
    const char *default_uart;

    node = of_find_node_by_name(NULL, "serial_led");
    if (!node) {
        pr_err("Failed to find device node\n");
        return -ENODEV;
    }

    ret = of_property_read_string(node, "default-uart", &default_uart);
    if (ret) {
        pr_err("Failed to read property 'default-uart'\n");
        return ret;
    }

    strncpy(serial_port_name, default_uart, sizeof(serial_port_name) - 1);
    serial_port_name[sizeof(serial_port_name) - 1] = '\0';

    INIT_DELAYED_WORK(&led_work, trigger_led_blink);
    serial_led_cdev = led_cdev;

    return 0;
}

static void serial_trig_deactivate(struct led_classdev *led_cdev)
{
    cancel_delayed_work_sync(&led_work);
}

void set_serial_led_blink(const char *port_name)
{
    if (strstr(serial_port_name, port_name) != NULL)
        schedule_delayed_work(&led_work, msecs_to_jiffies(0)); 
}

EXPORT_SYMBOL_GPL(set_serial_led_blink);

static struct led_trigger serial_led_trigger = {
    .name = "serial-activity",
    .activate = serial_trig_activate,
    .deactivate = serial_trig_deactivate,
    .groups = serial_trig_groups,
};

static int __init leds_serial_trigger_init(void)
{
    return led_trigger_register(&serial_led_trigger);
}

static void __exit leds_serial_trigger_exit(void)
{
    cancel_delayed_work_sync(&led_work);
    led_trigger_unregister(&serial_led_trigger);
}

module_init(leds_serial_trigger_init);
module_exit(leds_serial_trigger_exit);

MODULE_DESCRIPTION("Serial Activity LED Trigger");
MODULE_AUTHOR("lixin");
MODULE_LICENSE("GPL");