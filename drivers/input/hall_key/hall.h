#ifndef __HALL_H
#define __HALL_H
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
//Begin Modified by yan.gong for FR 10464744 on 2020-12-23
struct hall_data {
	int irq;
	int irq_gpio;
	int switch_code;
	struct input_dev *input;
	struct workqueue_struct *hall_wq;
	struct work_struct hall_work;
	struct platform_device	*pdev;
	int hall_status;
	int power_enabled;
	bool probe_flag;
#ifdef CONFIG_PM_WAKELOCKS
        struct wakeup_source hall_wakelock;
#else
        struct wake_lock hall_wakelock;
#endif
};
//End Modified by yan.gong for FR 10464744 on 2020-12-23
#endif


