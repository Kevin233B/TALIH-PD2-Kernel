// SPDX-License-Identifier: GPL-2.0
/*
 * fs/adh.c —— Android 应用存在性封堵（属主 uid 键控，in-tree）
 *
 * 语义目标：隐藏名单内应用的
 *   /storage/emulated/<n>/Android/{data,obb}/<pkg>
 *   /data/data/<pkg>、/data/user/<n>/<pkg>、/data/user_de/<n>/<pkg>
 * 对非属主普通应用（uid >= 10000）表现为“该应用未安装”：
 *   - stat/faccessat/open/exec/深路径/符号链接穿越  → ENOENT（与组件不存在一致）
 *   - mkdir/rename 目标/mknod/symlink/link/unlink/rmdir 等 create/delete 类
 *     末位组件 → 与“真不存在且父目录无写权限”逐字节一致（EACCES/ENOENT 按类），
 *     避免 fuse-bpf 生态已知的 mkdir oracle
 * 同时完全放行：属主自身、root/system/shell/media（uid < 10000 一条比较全覆盖）、
 * 预装系统应用（sys 集，由 service 解析 packages.xml 写入）、
 * 用户对该目标单独授权可见的应用（per-target allow 集）。
 *
 * 键控方式：按“目录属主 uid”而非路径名匹配——大小写/casefold 变体、零宽
 * 可忽略码点变体、应用重装（目录 inode 重建但 uid 不变）全部自然覆盖；
 * 且零 dcache 写入（不产生任何跨 uid 缓存污染，master:33 共享 dentry 树安全），
 * 零挂载拓扑变化（statfs/mountinfo 逐字节不变）。
 *
 * 配置平面（root 0600）：/sys/kernel/android_data_hide/rules
 *   parent <绝对路径>   受管父目录（Android/data、Android/obb、/data/data、
 *                       /data/user/<n>、/data/user_de/<n>；bind 多视图同
 *                       inode，(dev,ino) 键自然去重）
 *   hide <uid> [uid..]  隐藏目标（目录属主 uid）；行内附加 uid 为对该目标
 *                       单独授权可见的调用方
 *   allow <uid> [uid..] 全局放行（预装系统应用中 uid >= 10000 者）
 *   reset               丢弃未 commit 的缓冲
 *   commit              原子发布：RCU 换快照 + static_key 翻转
 *                       （parent 集与 hide 集均非空才启用，防半态生效；
 *                       无 pending 时 commit = 发布空态，即显式禁用）
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/uidgid.h>
#include <linux/rcupdate.h>
#include <linux/jump_label.h>
#include <linux/mutex.h>
#include <linux/lockdep.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/capability.h>
#include <linux/err.h>

#include "adh.h"

#define ADH_MAX_PARENTS		16
#define ADH_MAX_RULES		256
#define ADH_MAX_ALLOW_PER_RULE	16
#define ADH_MAX_SYS		1024

/* Android FIRST_APPLICATION_UID：应用 uid 恒 >= 10000。
 * 多用户应用（1xxxxx）、isolated（99000-99999）、app-zygote-isolated
 * （90000-98999）皆在拒绝域；0/1000/1023/2000 等系统 uid 一条比较全覆盖。 */
#define ADH_FIRST_APP_UID	10000

struct adh_parent {
	u32 dev;
	u64 ino;
};

struct adh_rule {
	kuid_t target;				/* 隐藏目标：目录属主 uid */
	u8 n_allow;
	kuid_t allow[ADH_MAX_ALLOW_PER_RULE];	/* 对该目标授权可见的调用方 */
};

struct adh_state {
	struct rcu_head rcu;
	u16 n_parents;
	u16 n_rules;
	u16 n_sys;
	struct adh_parent parents[ADH_MAX_PARENTS];
	struct adh_rule rules[ADH_MAX_RULES];
	kuid_t sys[ADH_MAX_SYS];
};

DEFINE_STATIC_KEY_FALSE(adh_key);

static struct adh_state __rcu *adh_active;
static struct adh_state *adh_pending;
static bool adh_enabled;
static DEFINE_MUTEX(adh_mutex);

bool __adh_should_hide(struct inode *parent, struct inode *child,
		       kuid_t caller)
{
	struct adh_state *s;
	u16 i, j;
	bool deny = false;

	if (!parent || !child)
		return false;

	rcu_read_lock();
	s = rcu_dereference(adh_active);
	if (!s)
		goto out;

	/* 属主自身放行（普通应用看自己的目录） */
	if (uid_eq(caller, child->i_uid))
		goto out;
	/* 非 uid>=10000 的调用方放行：root/system/shell/media 等一切系统侧 */
	if (caller.val < ADH_FIRST_APP_UID)
		goto out;

	/* 仅受管父目录之下的组件参与判定（bind 挂载多视图同 inode） */
	for (i = 0; i < s->n_parents; i++) {
		if (s->parents[i].dev == parent->i_sb->s_dev &&
		    s->parents[i].ino == (u64)parent->i_ino)
			break;
	}
	if (i == s->n_parents)
		goto out;

	/* 子目录属主 ∈ 隐藏目标集才拦（inode 属主 uid 键控） */
	for (j = 0; j < s->n_rules; j++) {
		if (uid_eq(s->rules[j].target, child->i_uid))
			break;
	}
	if (j == s->n_rules)
		goto out;

	/* 预装系统应用（uid >= 10000 者，service 写入）放行 */
	for (i = 0; i < s->n_sys; i++) {
		if (uid_eq(s->sys[i], caller))
			goto out;
	}
	/* 用户对该目标单独授权可见的应用放行 */
	for (i = 0; i < s->rules[j].n_allow; i++) {
		if (uid_eq(s->rules[j].allow[i], caller))
			goto out;
	}

	deny = true;
out:
	rcu_read_unlock();
	return deny;
}

/* ---------------- sysfs 配置平面 ---------------- */

static int adh_pending_alloc(void)
{
	if (adh_pending)
		return 0;
	adh_pending = kzalloc(sizeof(*adh_pending), GFP_KERNEL);
	if (!adh_pending)
		return -ENOMEM;
	return 0;
}

static int adh_add_parent(const char *path)
{
	struct path kp;
	u32 dev;
	u64 ino;
	u16 i;
	int ret;

	ret = kern_path(path, LOOKUP_FOLLOW, &kp);
	if (ret)
		return ret;

	if (!kp.dentry->d_inode) {
		path_put(&kp);
		return -ENOENT;
	}
	dev = kp.dentry->d_sb->s_dev;
	ino = kp.dentry->d_inode->i_ino;
	path_put(&kp);

	if (adh_pending->n_parents >= ADH_MAX_PARENTS)
		return -ENOSPC;
	for (i = 0; i < adh_pending->n_parents; i++) {
		if (adh_pending->parents[i].dev == dev &&
		    adh_pending->parents[i].ino == ino)
			return 0;	/* bind/同 inode 去重 */
	}
	adh_pending->parents[adh_pending->n_parents].dev = dev;
	adh_pending->parents[adh_pending->n_parents].ino = ino;
	adh_pending->n_parents++;
	return 0;
}

static int adh_add_rule(char *rest)
{
	struct adh_rule *r = NULL;
	char *tok;
	u32 v;
	u16 j;
	int ret;

	tok = strsep(&rest, " ");
	if (!tok || !*tok)
		return -EINVAL;
	ret = kstrtou32(tok, 10, &v);
	if (ret)
		return ret;

	/* 同 target 重复写 = 追加 allow */
	for (j = 0; j < adh_pending->n_rules; j++) {
		if (adh_pending->rules[j].target.val == v) {
			r = &adh_pending->rules[j];
			break;
		}
	}
	if (!r) {
		if (adh_pending->n_rules >= ADH_MAX_RULES)
			return -ENOSPC;
		r = &adh_pending->rules[adh_pending->n_rules++];
		r->target = KUIDT_INIT(v);
		r->n_allow = 0;
	}

	while (rest && (tok = strsep(&rest, " ")) != NULL) {
		if (!*tok)
			continue;
		ret = kstrtou32(tok, 10, &v);
		if (ret)
			return ret;
		if (r->n_allow >= ADH_MAX_ALLOW_PER_RULE)
			return -ENOSPC;
		r->allow[r->n_allow++] = KUIDT_INIT(v);
	}
	return 0;
}

static int adh_add_sys(char *rest)
{
	char *tok;
	u32 v;
	int ret;

	while (rest && (tok = strsep(&rest, " ")) != NULL) {
		if (!*tok)
			continue;
		ret = kstrtou32(tok, 10, &v);
		if (ret)
			return ret;
		if (adh_pending->n_sys >= ADH_MAX_SYS)
			return -ENOSPC;
		adh_pending->sys[adh_pending->n_sys++] = KUIDT_INIT(v);
	}
	return 0;
}

static void adh_state_free_rcu(struct rcu_head *rhc)
{
	kfree(container_of(rhc, struct adh_state, rcu));
}

static int adh_commit(void)
{
	struct adh_state *ns, *old;
	u16 n_parents, n_rules, n_sys;
	bool enable;

	/* 无 pending 时 commit = 显式发布空态（全可见/禁用）：
	 * uninstall.sh 以 reset+commit 清空退场。 */
	if (adh_pending) {
		ns = adh_pending;
	} else {
		ns = kzalloc(sizeof(*ns), GFP_KERNEL);
		if (!ns)
			return -ENOMEM;
	}
	adh_pending = NULL;

	enable = ns->n_parents && ns->n_rules;
	n_parents = ns->n_parents;
	n_rules = ns->n_rules;
	n_sys = ns->n_sys;

	old = rcu_dereference_protected(adh_active,
					lockdep_is_held(&adh_mutex));
	rcu_assign_pointer(adh_active, ns);
	if (old)
		call_rcu(&old->rcu, adh_state_free_rcu);

	if (enable && !adh_enabled) {
		static_branch_enable(&adh_key);
		adh_enabled = true;
	} else if (!enable && adh_enabled) {
		static_branch_disable(&adh_key);
		adh_enabled = false;
	}

	pr_info("adh: commit —— parents=%u rules=%u sys=%u enabled=%d\n",
		n_parents, n_rules, n_sys, enable);
	return 0;
}

static ssize_t adh_rules_show(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
	struct adh_state *s;
	int len = 0;
	u16 i, j;

	rcu_read_lock();
	s = rcu_dereference(adh_active);
	len += scnprintf(buf + len, PAGE_SIZE - len,
			 "enabled %d parents %u rules %u sys %u\n",
			 adh_enabled,
			 s ? s->n_parents : 0,
			 s ? s->n_rules : 0,
			 s ? s->n_sys : 0);
	if (!s)
		goto out;
	for (i = 0; i < s->n_parents && len < (int)(PAGE_SIZE - 64); i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "parent %u:%llu\n", s->parents[i].dev,
				 (unsigned long long)s->parents[i].ino);
	}
	for (j = 0; j < s->n_rules && len < (int)(PAGE_SIZE - 128); j++) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "hide %u", s->rules[j].target.val);
		for (i = 0; i < s->rules[j].n_allow; i++)
			len += scnprintf(buf + len, PAGE_SIZE - len,
					 " %u", s->rules[j].allow[i].val);
		len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	}
	for (i = 0; i < s->n_sys && len < (int)(PAGE_SIZE - 32); i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "allow %u\n", s->sys[i].val);
	}
out:
	rcu_read_unlock();
	return len;
}

static ssize_t adh_rules_store(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	char *lines, *line;
	int ret = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	lines = kmalloc(count + 1, GFP_KERNEL);
	if (!lines)
		return -ENOMEM;
	memcpy(lines, buf, count);
	lines[count] = '\0';

	mutex_lock(&adh_mutex);
	line = lines;
	while (line) {
		char *rest, *cmd;
		char *nl = strchr(line, '\n');

		if (nl)
			*nl = '\0';
		rest = strim(line);
		if (!*rest || *rest == '#')
			goto next;

		cmd = strsep(&rest, " ");
		if (!cmd || !*cmd) {
			ret = -EINVAL;
			break;
		}

		if (!strcmp(cmd, "reset")) {
			kfree(adh_pending);
			adh_pending = NULL;
		} else if (!strcmp(cmd, "commit")) {
			ret = adh_commit();
			if (ret)
				break;
		} else if (!strcmp(cmd, "parent")) {
			ret = adh_pending_alloc();
			if (!ret) {
				rest = rest ? strim(rest) : NULL;
				ret = rest ? adh_add_parent(rest) : -EINVAL;
			}
			if (ret)
				break;
		} else if (!strcmp(cmd, "hide")) {
			ret = adh_pending_alloc();
			if (!ret) {
				rest = rest ? strim(rest) : NULL;
				ret = rest ? adh_add_rule(rest) : -EINVAL;
			}
			if (ret)
				break;
		} else if (!strcmp(cmd, "allow")) {
			ret = adh_pending_alloc();
			if (!ret) {
				rest = rest ? strim(rest) : NULL;
				ret = rest ? adh_add_sys(rest) : -EINVAL;
			}
			if (ret)
				break;
		} else {
			ret = -EINVAL;
			break;
		}
next:
		line = nl ? nl + 1 : NULL;
	}
	mutex_unlock(&adh_mutex);
	kfree(lines);

	return ret ? ret : count;
}

static struct kobj_attribute adh_rules_attr =
	__ATTR(rules, 0600, adh_rules_show, adh_rules_store);

static struct kobject *adh_kobj;

static int __init adh_init(void)
{
	int ret;

	adh_kobj = kobject_create_and_add("android_data_hide", kernel_kobj);
	if (!adh_kobj)
		return -ENOMEM;

	ret = sysfs_create_file(adh_kobj, &adh_rules_attr.attr);
	if (ret) {
		kobject_put(adh_kobj);
		return ret;
	}

	pr_info("adh: ready —— 配置面 /sys/kernel/android_data_hide/rules（parent/hide/allow/reset/commit）\n");
	return 0;
}
late_initcall(adh_init);
