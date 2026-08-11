#ifndef __key_H
#define __key_H
#include <linux/notifier.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
//Begin Modified by yan.gong for FR 10464744 on 2020-12-23
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
#ifdef CONFIG_PM_WAKELOCKS
        struct wakeup_source key_wakelock;
#else
        struct wake_lock key_wakelock;
#endif
};
//End Modified by yan.gong for FR 10464744 on 2020-12-23
#endif


