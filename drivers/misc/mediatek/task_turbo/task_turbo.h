/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#ifndef _TASK_TURBO_H_
#define _TASK_TURBO_H_

#include <linux/list.h>
#include <linux/smp.h>

#define get_task_turbo_t(p)	\
	((struct task_turbo_t *)&(p)->android_vendor_data1)
#define get_inherit_task(parent)	\
	((struct task_struct *)((u64)(parent)->android_vendor_data1))

struct list_head;

enum {
	START_INHERIT = -1,
	RWSEM_INHERIT = 0,
	BINDER_INHERIT,
	END_INHERIT,
};

enum {
	SUB_FEAT_LOCK		= 1U << 0,
	SUB_FEAT_BINDER		= 1U << 1,
	SUB_FEAT_SCHED		= 1U << 2,
	SUB_FEAT_FLAVOR_BIGCORE = 1U << 3,
};

struct task_turbo_t {
	unsigned char turbo:1;
	unsigned char render:1;
	unsigned short inherit_cnt:14;
	short nice_backup;
	atomic_t inherit_types;
};

struct futex_q {
	struct plist_node list;

	struct task_struct *task;
	spinlock_t *lock_ptr;
	union futex_key key;
	struct futex_pi_state *pi_state;
	struct rt_mutex_waiter *rt_waiter;
	union futex_key *requeue_pi_key;
	u32 bitset;
} __randomize_layout;

struct hmp_domain {
	struct cpumask cpus;
	struct cpumask possible_cpus;
	struct list_head hmp_domains;
};

struct cluster_info {
	struct hmp_domain *hmpd;
	unsigned long cpu_perf;
	int cpu;
};

extern void list_sort(void *priv, struct list_head *head,
		int (*cmp)(void *priv, struct list_head *a,
			struct list_head *b));

/*
 * Nice levels are multiplicative, with a gentle 10% change for every
 * nice level changed. I.e. when a CPU-bound task goes from nice 0 to
 * nice 1, it will get ~10% less CPU time than another CPU-bound task
 * that remained on nice 0.
 *
 * The "10% effect" is relative and cumulative: from _any_ nice level,
 * if you go up 1 level, it's -10% CPU usage, if you go down 1 level
 * it's +10% CPU usage. (to achieve that we use a multiplier of 1.25.
 * If a task goes up by ~10% and another task goes down by ~10% then
 * the relative distance between them is ~25%.)
 */
extern const int sched_prio_to_weight[40];

/*
 * Inverse (2^32/x) values of the sched_prio_to_weight[] array, precalculated.
 *
 * In cases where the weight does not change often, we can use the
 * precalculated inverse to speed up arithmetics by turning divisions
 * into multiplications:
 */
extern const u32 sched_prio_to_wmult[40];

#endif /* _PERF_TRACKER_H */
