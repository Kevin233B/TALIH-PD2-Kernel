#!/system/bin/sh
# refresh.sh —— 全量发布配置到内核 sysfs（幂等；flock 串行所有发布会话）。
# 目标集为空时同样发布（等价“不封堵”），保持内核态与配置文件一致。
# 单次写入远小于 PAGE_SIZE：allow 行每行最多 20 个 uid。

MODDIR=${0%/*}
. "$MODDIR/common.sh"

exec 9>"$LOCK"
flock 9 || exit 1

if [ ! -e "$SYSFS" ]; then
    log "sysfs 不存在（内核补丁未刷）——跳过发布"
    exit 0
fi

wr() { echo "$1" > "$SYSFS" 2>/dev/null || { log "sysfs 写失败：$1"; exit 1; }; }

# ---- 系统放行集（uid>=10000 的系统应用）----
# 基础：packages.list 的 @system 行（明文、开机即可读）；
# 补充：pm（覆盖被商店更新过的系统应用——其 installer 标记不再是 @system）。
sys_uids=$(awk '$NF=="@system" && $2+0>=10000 {print $2+0}' "$PKG_LIST" 2>/dev/null)
pm_s=$(pm list packages -s -U 2>/dev/null)
if [ -n "$pm_s" ]; then
    : > "$PMREADY"
    pm_uids=$(printf '%s\n' "$pm_s" | sed -n 's/.*uid:\([0-9][0-9]*\)$/\1/p' | \
        awk '$1+0>=10000 {print $1+0}')
    sys_uids=$(printf '%s\n%s\n' "$sys_uids" "$pm_uids" | awk 'NF' | sort -nu)
else
    rm -f "$PMREADY" 2>/dev/null
fi
if [ -z "$sys_uids" ]; then
    log "系统放行集为空——解析异常，中止发布（保留上一态）"
    exit 1
fi

wr reset
for p in $PARENTS; do
    wr "parent $p"
done

n_hide=0
for pkg in $(list_pkgs "$HIDE_LIST"); do
    uid=$(resolve_uid "$pkg")
    if [ -z "$uid" ]; then
        log "目标未安装，跳过：$pkg"
        continue
    fi
    line="hide $uid"
    for caller in $(visible_callers_for "$pkg"); do
        cuid=$(resolve_uid "$caller")
        if [ -n "$cuid" ]; then
            line="$line $cuid"
        else
            log "授权方未安装，跳过：$caller"
        fi
    done
    wr "$line"
    n_hide=$((n_hide + 1))
done

n_sys=$(printf '%s\n' "$sys_uids" | awk 'NF' | wc -l)
buf=""
i=0
for u in $sys_uids; do
    if [ -z "$buf" ]; then buf="allow $u"; else buf="$buf $u"; fi
    i=$((i + 1))
    if [ "$i" -ge 20 ]; then
        wr "$buf"
        buf=""
        i=0
    fi
done
[ -n "$buf" ] && wr "$buf"

wr commit
touch "$STAMP"
log "已发布：hide=$n_hide sys=$n_sys"
