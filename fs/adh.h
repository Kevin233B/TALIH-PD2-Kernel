/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FS_ADH_H
#define _FS_ADH_H

#include <linux/types.h>
#include <linux/uidgid.h>

struct inode;

#ifdef CONFIG_ANDROID_DATA_HIDE

#include <linux/jump_label.h>

DECLARE_STATIC_KEY_FALSE(adh_key);
bool __adh_should_hide(struct inode *parent, struct inode *child,
		       kuid_t caller);

/*
 * namei 热路径唯一开销：一条 disabled 静态分支（名单为空时全系统零成本）。
 * 命中后才调用 fs/adh.c 的完整判定（RCU 读 + 属主 uid 键控规则匹配）。
 * CONFIG 关闭时编译期折叠为常量 false，namei 调用点不留任何痕迹。
 */
static inline bool adh_should_hide(struct inode *parent, struct inode *child,
				   kuid_t caller)
{
	if (static_branch_unlikely(&adh_key))
		return __adh_should_hide(parent, child, caller);
	return false;
}

#else /* !CONFIG_ANDROID_DATA_HIDE */

static inline bool adh_should_hide(struct inode *parent, struct inode *child,
				   kuid_t caller)
{
	return false;
}

#endif /* CONFIG_ANDROID_DATA_HIDE */

#endif /* _FS_ADH_H */
