#ifndef __key_H
#define __key_H
#include <linux/notifier.h>
#include <linux/pm_wakeup.h>
struct key_data {
	int irq;
	int irq_gpio;
	int keycode_up;
	int keycode_down;
	struct input_dev *input;
	struct workqueue_struct *key_wq;
	struct work_struct key_work;
	struct platform_device	*pdev;
	int key_status;
	int power_enabled;
	bool probe_flag;
	struct notifier_block fb_notif;
	struct wakeup_source key_wakelock;
};
#endif


