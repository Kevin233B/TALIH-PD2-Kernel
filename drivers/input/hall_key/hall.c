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
#include "hall.h"
#include <linux/interrupt.h>
#include <linux/slab.h>

#include <linux/of_irq.h>

static struct class *hall_class = NULL;
static void do_hall_work(struct work_struct *work)
{
        struct hall_data *hall = container_of(work, struct hall_data, hall_work);
        unsigned int gpio_status;
#ifdef CONFIG_PM_WAKELOCKS
    __pm_wakeup_event(hall->hall_wakelock, jiffies_to_msecs(HZ/2));
#endif
    gpio_status = gpio_get_value(hall->irq_gpio);
    input_event(hall->input, EV_SW, hall->switch_code, !gpio_status);
    input_event(hall->input, EV_SYN, 0, 0);
    printk("%s,%d,switch = %d,status = %d\n", __func__, __LINE__,
	    hall->switch_code, gpio_status);
    enable_irq(hall->irq);
}

static irqreturn_t interrupt_hall_irq(int irq, void *dev)
{
        struct platform_device *pdev = dev;
        struct hall_data *hall = platform_get_drvdata(pdev);

        if (hall->probe_flag == false)
         return IRQ_HANDLED;
    disable_irq_nosync(hall->irq);
    //printk("%s,%d,irq_value = %d\n",__func__,__LINE__, gpio_get_value(hall_eint_gpio));
        queue_work(hall->hall_wq, &hall->hall_work);
    return IRQ_HANDLED;
}

static ssize_t hall_status_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
        struct hall_data *hall = dev_get_drvdata(dev);
    hall->hall_status = gpio_get_value(hall->irq_gpio);
    sprintf(buf,"%d\n", hall->hall_status);
    return strlen(buf);
}

static DEVICE_ATTR(hall_status, 0444, hall_status_show, NULL);

static int hall_probe(struct platform_device *pdev)
{
        struct hall_data *hall = NULL;
        struct device_node *np = pdev->dev.of_node;
        struct device *hall_dev = NULL;
	int gpio_status;
        int rc = 0;
	if (!np)
		return -ENODEV;
        hall = devm_kzalloc(&pdev->dev, sizeof(*hall), GFP_KERNEL);
        if (hall == NULL) {
                printk(KERN_INFO"%s:%d Unable to allocate memory\n", __func__, __LINE__);
                return -ENOMEM;
        }
        hall->irq_gpio = of_get_named_gpio(np, "hall-gpio", 0);
        hall->irq = irq_of_parse_and_map(np, 0);
        hall->pdev = pdev;
        dev_set_drvdata(&pdev->dev, hall);

        rc = request_threaded_irq(hall->irq, interrupt_hall_irq, NULL,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				pdev->name, pdev);
        if (rc) {
            rc = -1;
            printk("%s : requesting IRQ error\n", __func__);
            return rc;
        } else {
            printk("%s : requesting IRQ %d\n", __func__, hall->irq);
        }



        hall->input = input_allocate_device();
        if (!hall->input) {
                printk("hall.c: Not enough memory\n");
                return -ENOMEM;
        }
        hall->input->name = pdev->name;
        if (of_property_read_u32(np, "linux,hall", &hall->switch_code)) {
                printk("KEY_HALL_SENSOR without setting in dts\n");
                return -EINVAL;
        }
        input_set_capability(hall->input, EV_SW, hall->switch_code);
    rc = input_register_device(hall->input);
    if (rc) {
        printk("hall.c: Failed to register device\n");
        return rc;
    }

    hall->hall_wq = create_singlethread_workqueue("hall_wq");
    if (!hall->hall_wq) {
          printk(KERN_CRIT"%s: create thread error!\n", __func__);
    }

    INIT_WORK(&hall->hall_work, do_hall_work);
    irq_set_irq_wake(hall->irq, 1);
     if (!hall_class)
    hall_class= class_create(THIS_MODULE, "hall_switch");
    hall_dev = device_create(hall_class, NULL, 0, hall, pdev->name);
    if (IS_ERR(hall_dev))
            printk( "Failed to create device(hall_dev)!\n");

    if (device_create_file(hall_dev, &dev_attr_hall_status) < 0)
            printk( "Failed to create device(hall_dev)'s node hall_status!\n");
#ifdef CONFIG_PM_WAKELOCKS
    hall->hall_wakelock = wakeup_source_register(&pdev->dev, pdev->name);
#endif

    printk("hall probe completed\n");
    gpio_status = gpio_get_value(hall->irq_gpio);
    input_event(hall->input, EV_SW, hall->switch_code, !gpio_status);
    input_event(hall->input, EV_SYN, 0, 0);
    hall->probe_flag = true;
    return 0;
}

static int hall_remove(struct platform_device *pdev)
{
    struct hall_data *hall = platform_get_drvdata(pdev);

    free_irq(hall->irq, pdev);
#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_unregister(hall->hall_wakelock);
#endif

    input_unregister_device(hall->input);
    if (hall->input)
    {
        input_free_device(hall->input);
        hall->input = NULL;
    }

    return 0;
}

static struct of_device_id hall_of_match[] = {
    {.compatible = "mediatek,hall_switch_1"  },
    {.compatible = "mediatek,hall_switch_2" },
    {},
};

static struct platform_driver hall_driver = {
    .probe = hall_probe,
    .remove = hall_remove,
    .driver = {
           .name = "hall_switch",
           .owner = THIS_MODULE,
           .of_match_table = hall_of_match,
    }
};

static int __init hall_init(void)
{
    return platform_driver_register(&hall_driver);
}

static void __init hall_exit(void)
{
    platform_driver_unregister(&hall_driver);
}

module_init(hall_init);
module_exit(hall_exit);
MODULE_LICENSE("GPL");
