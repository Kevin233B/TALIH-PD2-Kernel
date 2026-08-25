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
#include <linux/of_fdt.h>
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
	int (*pen_bat_report)(char *buf, int *battery, int mode);
	int (*charge_process_set)(int enable);
	int (*grip_set)(int screen_mode, int grip_level);
	int (*aod_set)(int enable);
	int (*current_boost_set)(int enable);
	int (*night_mode_set)(int enable);
	u8 double_wakeup_en;
	u8 singleclick_en;
	u8 prox_en;
	u8 charge_process_en;
	u8 aod_en;
	u8 current_boost_en;
	u8 night_mode_en;
	int pen_bat_mode;
	int screen_mode;
	int grip_level;
	struct mutex grip_lock;
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

int tct_devinfo_register_pen_bat(int (*cb)(char *buf, int *battery, int mode))
{
	tct_ctrl.pen_bat_report = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_pen_bat);

int tct_devinfo_register_charge_process(int (*cb)(int enable))
{
	tct_ctrl.charge_process_set = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_charge_process);

int tct_devinfo_register_grip(int (*cb)(int screen_mode, int grip_level))
{
	tct_ctrl.grip_set = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_grip);

int tct_devinfo_register_aod(int (*cb)(int enable))
{
	tct_ctrl.aod_set = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_aod);

int tct_devinfo_register_current_boost(int (*cb)(int enable))
{
	tct_ctrl.current_boost_set = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_current_boost);

int tct_devinfo_register_night_mode(int (*cb)(int enable))
{
	tct_ctrl.night_mode_set = cb;
	return 0;
}
EXPORT_SYMBOL(tct_devinfo_register_night_mode);

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

static struct tct_deviceinfo_node *tct_deviceinfo_find_node(const char *name)
{
	struct tct_deviceinfo_node *node;

	list_for_each_entry(node, &tct_deviceinfo_nodes, list)
		if (!strcmp(node->name, name))
			return node;
	return NULL;
}

/*
 * TCT CPU devinfo, reconstructed from the OEM kernel: when the DT machine
 * name matches, write the full CPU info string into the "CPU" MMI node.
 */
void set_cpu_devinfo(const char *name)
{
	struct tct_deviceinfo_node *node;
	char buf[64];

	if (!name || strcmp(name, "MT8797Z/CNZA"))
		return;

	snprintf(buf, sizeof(buf), "%s:%s:%s:%s",
		 "MT8797Z/CNZA", "MTK", "NULL", "AMA0001228C1");
	node = tct_deviceinfo_find_node("CPU");
	if (node)
		strlcpy(node->value, buf, sizeof(node->value));
}
EXPORT_SYMBOL(set_cpu_devinfo);

int tct_cpu_devinfo_init(void)
{
	const char *name = of_flat_dt_get_machine_name();

	if (!name)
		return -ENODEV;

	set_cpu_devinfo(name);
	return 0;
}
EXPORT_SYMBOL(tct_cpu_devinfo_init);

void tct_cpu_devinfo_exit(void)
{
}
EXPORT_SYMBOL(tct_cpu_devinfo_exit);

late_initcall(tct_cpu_devinfo_init);

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
	int ret;

	if (!tct_ctrl.pen_bat_report)
		return 0;

	ret = tct_ctrl.pen_bat_report(tmp, &battery, tct_ctrl.pen_bat_mode);
	if (ret)
		return 0;
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

static ssize_t charge_process_enable_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", tct_ctrl.charge_process_en);
}

static ssize_t charge_process_enable_store(struct device *dev,
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

	if (tct_ctrl.charge_process_set)
		tct_ctrl.charge_process_set(val);
	tct_ctrl.charge_process_en = val;
	return count;
}
static DEVICE_ATTR(charger_mode, 0644, charge_process_enable_show,
		   charge_process_enable_store);

static ssize_t tct_grip_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "screen_mode=%d, grip_level=%d\n",
			tct_ctrl.screen_mode, tct_ctrl.grip_level);
}

static ssize_t tct_grip_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int screen_mode, grip_level;
	int prev_mode, prev_level;

	mutex_lock(&tct_ctrl.grip_lock);
	prev_mode = tct_ctrl.screen_mode;
	prev_level = tct_ctrl.grip_level;
	if (sscanf(buf, "%d,%d", &screen_mode, &grip_level) != 2) {
		mutex_unlock(&tct_ctrl.grip_lock);
		return -EINVAL;
	}

	pr_info("\x016%s, val=%d,%d previos screen_mode=%d, grip_level=%d\n",
		"tct_grip_mode_store", screen_mode, grip_level,
		prev_mode, prev_level);
	tct_ctrl.screen_mode = screen_mode;
	tct_ctrl.grip_level = grip_level;

	if (tct_ctrl.grip_set)
		tct_ctrl.grip_set(screen_mode, grip_level);
	mutex_unlock(&tct_ctrl.grip_lock);

	return count;
}
static DEVICE_ATTR(grip_mode, 0644, tct_grip_mode_show, tct_grip_mode_store);

static ssize_t aod_process_enable_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", tct_ctrl.aod_en);
}

static ssize_t aod_process_enable_store(struct device *dev,
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

	if (tct_ctrl.aod_set)
		tct_ctrl.aod_set(val);
	tct_ctrl.aod_en = val;
	return count;
}
static DEVICE_ATTR(aod_mode, 0644, aod_process_enable_show,
		   aod_process_enable_store);

static ssize_t current_boost_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", tct_ctrl.current_boost_en);
}

static ssize_t current_boost_store(struct device *dev,
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

	if (tct_ctrl.current_boost_set)
		tct_ctrl.current_boost_set(val);
	tct_ctrl.current_boost_en = val;
	return count;
}
static DEVICE_ATTR_RW(current_boost);

static ssize_t night_mode_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", tct_ctrl.night_mode_en);
}

static ssize_t night_mode_store(struct device *dev,
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

	if (tct_ctrl.night_mode_set)
		tct_ctrl.night_mode_set(val);
	tct_ctrl.night_mode_en = val;
	return count;
}
static DEVICE_ATTR_RW(night_mode);

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

static const struct device_attribute * const tct_deviceinfo_ctrl_attrs[] = {
	&dev_attr_double_wakeup_enable,
	&dev_attr_prox_enable,
	&dev_attr_prox_status,
	&dev_attr_singleclick_wakeup_enable,
	&dev_attr_penbat,
	&dev_attr_charger_mode,
	&dev_attr_grip_mode,
	&dev_attr_aod_mode,
	&dev_attr_current_boost,
	&dev_attr_night_mode,
};

static int __init deviceinfo_init(void)
{
	int i, ret;

	mutex_init(&tct_ctrl.grip_lock);

	ret = Create_tct_all_deviceinfo_node_ForMMI();
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(tct_deviceinfo_mmi_nodes); i++) {
		ret = tct_deviceinfo_create_node(tct_deviceinfo_mmi_nodes[i]);
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

	/*
	 * 官核框架(services.jar)固定读写 /sys/devices/virtual/tct_touch/
	 * tct_touch_dev/singleclick_wakeup_enable, 与重建驱动的 deviceinfo
	 * 路径不同, 导致轻触唤醒开关永远写不进内核. 这里按官核路径额外
	 * 暴露同名节点(与上面节点共享同一 singleclick_set 回调/状态).
	 */
	{
		struct class *tct_touch_class =
			class_create(THIS_MODULE, "tct_touch");
		struct device *tct_touch_dev;

		if (!IS_ERR(tct_touch_class)) {
			tct_touch_dev = device_create(tct_touch_class,
						      NULL, 0, NULL,
						      "tct_touch_dev");
			if (!IS_ERR(tct_touch_dev)) {
				ret = device_create_file(tct_touch_dev,
					&dev_attr_singleclick_wakeup_enable);
				if (ret < 0)
					pr_err("Failed to create tct_touch node: %d\n",
					       ret);
			}
		}
	}

	pr_info("TCT deviceinfo nodes created\n");
	return 0;
}
device_initcall(deviceinfo_init);

MODULE_AUTHOR("TALPAD-BOOM / reconstructed from OEM kernel");
MODULE_DESCRIPTION("TCT deviceinfo nodes for TALPAD MMI tests");
MODULE_LICENSE("GPL v2");
