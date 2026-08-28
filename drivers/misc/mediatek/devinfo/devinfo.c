// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * 5.10 精简移植：仅保留 get_devinfo_with_index()，读取 bootloader 填入
 * /chosen 节点 atag,devinfo 的 devinfo 表（每个 u32 索引对应 segment/efuse
 * 信息），供 gpufreq / eemgpu / mt6577_auxadc 等驱动读取 efuse 值。
 *
 * 4.19 的 char dev / debugfs / HRID / devinfo_ready 等 API 在 5.10 无调用者，
 * 不移植（对齐 TCT 用 sysfs deviceinfo 替代 MTK char dev 的现状）。
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/atomic.h>

#define DEVINFO_UNINIT		0
#define DEVINFO_INITIALIZED	1

/* atag,devinfo 表头，data[0] 为柔性数组成员 */
struct devinfo_tag {
	u32 data_size;
	u32 data[0];
};

static u32 *g_devinfo_data;
static u32 g_devinfo_size;
static atomic_t g_devinfo_init_status = ATOMIC_INIT(DEVINFO_UNINIT);

static u32 devinfo_get_size(void)
{
	return g_devinfo_size;
}

static void devinfo_parse_dt(void)
{
	struct device_node *chosen_node;
	const struct devinfo_tag *tags;
	u32 size;

	chosen_node = of_find_node_by_path("/chosen");
	if (!chosen_node)
		chosen_node = of_find_node_by_path("/chosen@0");
	if (!chosen_node) {
		pr_err("[devinfo] chosen node is not found\n");
		return;
	}

	tags = of_get_property(chosen_node, "atag,devinfo", NULL);
	if (!tags) {
		pr_err("[devinfo] 'atag,devinfo' is not found\n");
		return;
	}

	size = tags->data_size;
	WARN_ON(size > 300U); /* 防 size 过大导致越界 */

	g_devinfo_data = kmalloc(size * sizeof(u32), GFP_KERNEL);
	if (!g_devinfo_data) {
		pr_err("[devinfo] kmalloc fail\n");
		return;
	}

	g_devinfo_size = size;
	memcpy(g_devinfo_data, tags->data, size * sizeof(u32));
}

static void init_devinfo_exclusive(void)
{
	if (atomic_read(&g_devinfo_init_status) == DEVINFO_INITIALIZED) {
		pr_err("[devinfo] already initialized earlier\n");
		return;
	}

	if (atomic_read(&g_devinfo_init_status) == DEVINFO_UNINIT)
		atomic_set(&g_devinfo_init_status, DEVINFO_INITIALIZED);
	else
		return;

	devinfo_parse_dt();
}

u32 get_devinfo_with_index(u32 index)
{
	u32 size = devinfo_get_size();
	u32 ret = 0;

	if (size == 0) {
		/* 调用可能早于 devinfo 数据就绪，此时惰性初始化 */
		init_devinfo_exclusive();
		size = devinfo_get_size();
	}

	if ((index < size) && (g_devinfo_data != NULL))
		ret = g_devinfo_data[index];
	else {
		pr_err("[devinfo] index %u out of range (size %u)\n",
		       index, size);
		ret = 0xFFFFFFFF;
	}

	return ret;
}
EXPORT_SYMBOL(get_devinfo_with_index);
