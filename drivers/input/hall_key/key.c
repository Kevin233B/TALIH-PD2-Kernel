#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/input/mt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include "key.h"
#include <linux/interrupt.h>
#include <linux/slab.h>

#include <linux/of_irq.h>
#include <linux/fb.h>

/*
 * TCT addition, reconstructed from the OEM kernel: the key GPIO is only
 * reported while the display is blanked (FB_EARLY_EVENT_BLANK/POWERDOWN).
 */
static bool tct_key_screen_off;

static int tct_key_hid_fb_notifier_callback(struct notifier_block *nb,
					    unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (!evdata || !evdata->data)
		return 0;

	blank = evdata->data;

	if (event == FB_EARLY_EVENT_BLANK && *blank == FB_BLANK_POWERDOWN) {
		tct_key_screen_off = true;
		pr_info("%s: screen off, blank=%d\n", __func__, *blank);
	} else if (event == FB_EVENT_BLANK && *blank == FB_BLANK_UNBLANK) {
		tct_key_screen_off = false;
		pr_info("%s: screen on, blank=%d\n", __func__, *blank);
	}

	return 0;
}

static struct class *key_class = NULL;
static void do_key_work(struct work_struct *work)
{
        struct key_data *key = container_of(work, struct key_data, key_work);
        unsigned int gpio_status;
	int gpio_status2;
#ifdef CONFIG_PM_WAKELOCKS
    __pm_wakeup_event(key->key_wakelock, jiffies_to_msecs(HZ/2));
#endif
    gpio_status = gpio_get_value(key->irq_gpio);
    msleep(50);
    gpio_status2 = gpio_get_value(key->irq_gpio);

    /*
     * Debounce and only report while the display is off, matching the OEM
     * kernel: the key must be stable high for 50 ms and the screen must be
     * blanked before a KEY_POWER event is emitted.
     */
    if (gpio_status != gpio_status2 || gpio_status != 1) {
	    msleep(800);
	    enable_irq(key->irq);
	    return;
    }

    if (!tct_key_screen_off) {
	    msleep(800);
	    enable_irq(key->irq);
	    return;
    }

    input_report_key(key->input, key->keycode_down, 1);
    input_sync(key->input);
    input_report_key(key->input, key->keycode_down, 0);
    input_sync(key->input);
    printk("%s,%d,keycode = %d,report key\n",__func__,__LINE__,key->keycode_down);
    msleep(800);
    enable_irq(key->irq);
}

static irqreturn_t interrupt_key_irq(int irq, void *dev)
{
        struct platform_device *pdev = dev;
        struct key_data *key = platform_get_drvdata(pdev);

        if (key->probe_flag == false)
         return IRQ_HANDLED;
    disable_irq_nosync(key->irq);
    //printk("%s,%d,irq_value = %d\n",__func__,__LINE__, gpio_get_value(key_eint_gpio));
        queue_work(key->key_wq, &key->key_work);
    return IRQ_HANDLED;
}

static ssize_t key_status_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
        struct key_data *key = dev_get_drvdata(dev);
    key->key_status = gpio_get_value(key->irq_gpio);
    sprintf(buf,"%d\n", key->key_status);
    return strlen(buf);
}

static DEVICE_ATTR(key_status, 0444, key_status_show, NULL);

static int key_probe(struct platform_device *pdev)
{
        struct key_data *key = NULL;
        struct device_node *np = pdev->dev.of_node;
        struct device *key_dev = NULL;
        int rc = 0;
	if (!np)
		return -ENODEV;
        key = devm_kzalloc(&pdev->dev, sizeof(*key), GFP_KERNEL);
        if (key == NULL) {
                printk(KERN_INFO"%s:%d Unable to allocate memory\n", __func__, __LINE__);
                return -ENOMEM;
        }
        key->irq_gpio = of_get_named_gpio(np, "key-gpio", 0);
        key->irq = irq_of_parse_and_map(np, 0);
        key->pdev = pdev;
        dev_set_drvdata(&pdev->dev, key);

        rc = request_threaded_irq(key->irq, interrupt_key_irq, NULL,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				pdev->name, pdev);
        if (rc) {
            rc = -1;
            printk("%s : requesting IRQ error\n", __func__);
            return rc;
        } else {
            printk("%s : requesting IRQ %d\n", __func__, key->irq);
        }



        key->input = input_allocate_device();
        if (!key->input) {
                printk("key.c: Not enough memory\n");
                return -ENOMEM;
        }
        key->input->name = pdev->name;
        if (of_property_read_u32(np, "linux,keycode_down", &key->keycode_down)) {
                key->keycode_down = KEY_POWER;
                printk("KEY_key_SENSOR without setting in dts\n");
        }
        input_set_capability(key->input, EV_KEY, key->keycode_down);
    rc = input_register_device(key->input);
    if (rc) {
        printk("key.c: Failed to register device\n");
        return rc;
    }

    key->key_wq = create_singlethread_workqueue("key_wq");
    if (!key->key_wq) {
          printk(KERN_CRIT"%s: create thread error!\n", __func__);
    }

    INIT_WORK(&key->key_work, do_key_work);
    enable_irq_wake(key->irq);
    key->fb_notif.notifier_call = tct_key_hid_fb_notifier_callback;
    rc = fb_register_client(&key->fb_notif);
    if (rc) {
        printk("key.c: Failed to register fb notifier, rc = %d\n", rc);
        return rc;
    }
     if (!key_class)
    key_class= class_create(THIS_MODULE, "key_switch");
    key_dev = device_create(key_class, NULL, 0, key, pdev->name);
    if (IS_ERR(key_dev))
            printk( "Failed to create device(key_dev)!\n");

    if (device_create_file(key_dev, &dev_attr_key_status) < 0)
            printk( "Failed to create device(key_dev)'s node key_status!\n");
#ifdef CONFIG_PM_WAKELOCKS
    key->key_wakelock = wakeup_source_register(&pdev->dev, pdev->name);
#endif

    printk("key probe completed\n");
    key->probe_flag = true;
    return 0;
}

static int key_remove(struct platform_device *pdev)
{
    struct key_data *key = platform_get_drvdata(pdev);

    fb_unregister_client(&key->fb_notif);
    free_irq(key->irq, pdev);
#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_unregister(key->key_wakelock);
#endif

    input_unregister_device(key->input);
    if (key->input)
    {
        input_free_device(key->input);
        key->input = NULL;
    }

    return 0;
}

static struct of_device_id key_of_match[] = {
    {.compatible = "mediatek,key_switch_1"  },
    {.compatible = "mediatek,key_switch_2" },
    {},
};

static struct platform_driver key_driver = {
    .probe = key_probe,
    .remove = key_remove,
    .driver = {
           .name = "key_switch",
           .owner = THIS_MODULE,
           .of_match_table = key_of_match,
    }
};

static int __init volumekey_init(void)
{
    return platform_driver_register(&key_driver);
}

static void __init volumekey_exit(void)
{
    platform_driver_unregister(&key_driver);
}

module_init(volumekey_init);
module_exit(volumekey_exit);
MODULE_LICENSE("GPL");
