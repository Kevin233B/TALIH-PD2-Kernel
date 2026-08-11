// SPDX-License-Identifier: GPL-2.0
/*
 * TCT deviceinfo driver for TALPAD (TALIH-PD2, ls12_mt8797_wifi_64)
 *
 * Reconstructed from the OEM kernel 4.19.191+ (2026-03-26 build).
 *
 * Creates /sys/class/deviceinfo/device_info/... nodes used by factory/MMI
 * tests. The aggregate node "tct_all_deviceinfo" dumps all registered
 * entries as "name:value;" pairs, matching the OEM behavior. The per-node
 * show/store access generic string buffers, which is how the OEM info
 * nodes behave (CamName*, CamOTP*, LCM, sensors, speakers, etc.).
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#define TCT_DEVICEINFO_CLASS_NAME	"deviceinfo"
#define TCT_DEVICEINFO_DEV_NAME		"device_info"
#define TCT_DEVICEINFO_VALUE_MAX	64

static struct class *tct_deviceinfo_class;
static struct device *tct_deviceinfo_dev;
static DEFINE_MUTEX(tct_deviceinfo_mutex);

struct tct_deviceinfo_node {
	struct list_head list;
	char name[32];
	char value[TCT_DEVICEINFO_VALUE_MAX];
	struct device_attribute dev_attr;
};

static LIST_HEAD(tct_deviceinfo_nodes);

/*
 * Control bridge between TCT_DEVICEINFO and the touch driver, mirroring the
 * OEM kernel: the touch driver registers callbacks during probe and the
 * sysfs nodes below invoke them.
 */
struct tct_devinfo_ctrl {
	int (*double_wakeup_set)(int enable);
	int (*singleclick_set)(struct device *dev, int enable);
	int (*prox_set)(int enable);
	void (*pen_bat_report)(char *buf, int *battery, int mode);
	u8 double_wakeup_en;
	u8 singleclick_en;
	u8 prox_en;
	int pen_bat_mode;
};

static struct tct_devinfo_ctrl tct_ctrl;

int tct_devinfo_register_double_wakeup(int (*cb)(int enable))
{
	tct_ctrl.double_wakeup_set = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_double_wakeup);

int tct_devinfo_register_singleclick(int (*cb)(struct device *dev, int enable))
{
	tct_ctrl.singleclick_set = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_singleclick);

int tct_devinfo_register_prox(int (*cb)(int enable))
{
	tct_ctrl.prox_set = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_prox);

int tct_devinfo_register_pen_bat(void (*cb)(char *buf, int *battery, int mode))
{
	tct_ctrl.pen_bat_report = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_pen_bat);

struct device *get_deviceinfo_dev(void)
{
	if (tct_deviceinfo_dev)
		return tct_deviceinfo_dev;

	mutex_lock(&tct_deviceinfo_mutex);
	if (!tct_deviceinfo_class) {
		tct_deviceinfo_class = class_create(THIS_MODULE,
						    TCT_DEVICEINFO_CLASS_NAME);
		if (IS_ERR(tct_deviceinfo_class)) {
			pr_err("Failed to create class(%s)!\n",
			       TCT_DEVICEINFO_CLASS_NAME);
			tct_deviceinfo_class = NULL;
			mutex_unlock(&tct_deviceinfo_mutex);
			return NULL;
		}
	}

	if (!tct_deviceinfo_dev) {
		tct_deviceinfo_dev = device_create(tct_deviceinfo_class,
						   NULL, 0, NULL,
						   TCT_DEVICEINFO_DEV_NAME);
		if (IS_ERR(tct_deviceinfo_dev)) {
			pr_err("Failed to create device(%s)!\n",
			       TCT_DEVICEINFO_DEV_NAME);
			tct_deviceinfo_dev = NULL;
			class_destroy(tct_deviceinfo_class);
			tct_deviceinfo_class = NULL;
			mutex_unlock(&tct_deviceinfo_mutex);
			return NULL;
		}
	}

	mutex_unlock(&tct_deviceinfo_mutex);
	return tct_deviceinfo_dev;
}
EXPORT_SYMBOL(get_deviceinfo_dev);

static ssize_t tct_deviceinfo_generic_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tct_deviceinfo_node *node =
		container_of(attr, struct tct_deviceinfo_node, dev_attr);

	return snprintf(buf, PAGE_SIZE, "%s", node->value);
}

static ssize_t tct_deviceinfo_generic_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct tct_deviceinfo_node *node =
		container_of(attr, struct tct_deviceinfo_node, dev_attr);
	size_t len = count;

	if (len >= TCT_DEVICEINFO_VALUE_MAX)
		len = TCT_DEVICEINFO_VALUE_MAX - 1;
	memcpy(node->value, buf, len);
	node->value[len] = '\0';

	return count;
}

static ssize_t tct_all_deviceinfo_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct tct_deviceinfo_node *node;
	size_t len = 0;

	list_for_each_entry(node, &tct_deviceinfo_nodes, list) {
		int ret;

		if (len >= PAGE_SIZE - 1)
			break;
		ret = snprintf(buf + len, PAGE_SIZE - len, "%s:%s;",
			       node->name, node->value);
		if (ret < 0)
			break;
		len += ret;
		if (len > PAGE_SIZE - 1)
			len = PAGE_SIZE - 1;
	}

	buf[len++] = '\n';
	buf[len] = '\0';
	return len;
}

static ssize_t tct_all_deviceinfo_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR_RW(tct_all_deviceinfo);

static ssize_t double_wakeup_enable_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", tct_ctrl.double_wakeup_en);
}

static ssize_t double_wakeup_enable_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;
	if (val > 1)
		return -EINVAL;

	if (tct_ctrl.double_wakeup_set)
		tct_ctrl.double_wakeup_set(val);
	tct_ctrl.double_wakeup_en = val;
	return count;
}
static DEVICE_ATTR_RW(double_wakeup_enable);

static ssize_t singleclick_wakeup_enable_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", tct_ctrl.singleclick_en);
}

static ssize_t singleclick_wakeup_enable_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;
	if (val > 1)
		return -EINVAL;

	if (tct_ctrl.singleclick_set)
		tct_ctrl.singleclick_set(NULL, val);
	tct_ctrl.singleclick_en = val;
	return count;
}
static DEVICE_ATTR_RW(singleclick_wakeup_enable);

static ssize_t prox_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", tct_ctrl.prox_en);
}

static ssize_t prox_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;
	if (val > 1)
		return -EINVAL;

	if (tct_ctrl.prox_set)
		tct_ctrl.prox_set(val);
	tct_ctrl.prox_en = val;
	return count;
}
static DEVICE_ATTR_RW(prox_enable);

static ssize_t prox_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", tct_ctrl.prox_en);
}

static ssize_t prox_status_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	return count;
}
static DEVICE_ATTR_RW(prox_status);

static ssize_t tct_penbat_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	char tmp[64] = {0};
	int battery = 0;

	if (!tct_ctrl.pen_bat_report)
		return 0;

	tct_ctrl.pen_bat_report(tmp, &battery, tct_ctrl.pen_bat_mode);
	return snprintf(buf, PAGE_SIZE, "%s\n", tmp);
}

static ssize_t tct_penbat_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int val, ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;

	tct_ctrl.pen_bat_mode = val;
	return count;
}
static DEVICE_ATTR(penbat, 0644, tct_penbat_show, tct_penbat_store);

int Create_tct_all_deviceinfo_node_ForMMI(void)
{
	struct device *dev = get_deviceinfo_dev();
	int ret;

	if (!dev)
		return -ENODEV;

	ret = device_create_file(dev, &dev_attr_tct_all_deviceinfo);
	if (ret < 0)
		pr_err("Failed to create tct_all_deviceinfo node: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL(Create_tct_all_deviceinfo_node_ForMMI);

static int tct_deviceinfo_create_node(const char *name)
{
	struct tct_deviceinfo_node *node;
	struct device *dev;
	int ret;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	strlcpy(node->name, name, sizeof(node->name));
	sysfs_attr_init(&node->dev_attr.attr);
	node->dev_attr.attr.name = node->name;
	node->dev_attr.attr.mode = 0644;
	node->dev_attr.show = tct_deviceinfo_generic_show;
	node->dev_attr.store = tct_deviceinfo_generic_store;

	dev = get_deviceinfo_dev();
	if (!dev) {
		kfree(node);
		return -ENODEV;
	}

	ret = device_create_file(dev, &node->dev_attr);
	if (ret < 0) {
		pr_err("Failed to create %s node: %d\n", name, ret);
		kfree(node);
		return ret;
	}

	list_add_tail(&node->list, &tct_deviceinfo_nodes);
	return 0;
}

/* MMI info nodes, same names as the OEM kernel */
static const char * const tct_deviceinfo_mmi_nodes[] = {
	"CamNameB", "CamNameF", "CamNameB2", "CamNameF2",
	"CamNameB3", "CamNameB4",
	"CamOTPB", "CamOTPF", "CamOTPB2", "CamOTPF2",
	"CamOTPB3", "CamOTPB4",
	"LCM", "ctp", "eMMC", "gsensor", "psensor", "lsensor",
	"gyroscope", "compass", "NFC", "battery_info",
	"speaker1", "speaker2", "speaker3", "speaker4",
	"receiver1", "receiver2", "FM", "hall1", "hall2",
	"bluetooth", "wifi", "gps", "DTV", "ATV", "CPU", "fp",
	"DDR", "charger",
};

/* Control nodes exposed by the OEM sysfs groups */
static const char * const tct_deviceinfo_ctrl_nodes[] = {
	"charger_mode", "grip_mode", "aod_mode",
};

static const struct device_attribute * const tct_deviceinfo_ctrl_attrs[] = {
	&dev_attr_double_wakeup_enable,
	&dev_attr_prox_enable,
	&dev_attr_prox_status,
	&dev_attr_singleclick_wakeup_enable,
	&dev_attr_penbat,
};

static int __init deviceinfo_init(void)
{
	int i, ret;

	ret = Create_tct_all_deviceinfo_node_ForMMI();
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(tct_deviceinfo_mmi_nodes); i++) {
		ret = tct_deviceinfo_create_node(tct_deviceinfo_mmi_nodes[i]);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(tct_deviceinfo_ctrl_nodes); i++) {
		ret = tct_deviceinfo_create_node(tct_deviceinfo_ctrl_nodes[i]);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(tct_deviceinfo_ctrl_attrs); i++) {
		struct device *dev = get_deviceinfo_dev();

		if (!dev)
			return -ENODEV;
		ret = device_create_file(dev, tct_deviceinfo_ctrl_attrs[i]);
		if (ret < 0) {
			pr_err("Failed to create ctrl node: %d\n", ret);
			return ret;
		}
	}

	pr_info("TCT deviceinfo nodes created\n");
	return 0;
}
device_initcall(deviceinfo_init);

MODULE_AUTHOR("TALPAD-BOOM / reconstructed from OEM kernel");
MODULE_DESCRIPTION("TCT deviceinfo nodes for TALPAD MMI tests");
MODULE_LICENSE("GPL v2");
