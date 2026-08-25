#ifndef __HALL_H
#define __HALL_H
#include <linux/pm_wakeup.h>
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
	struct wakeup_source hall_wakelock;
};
#endif


