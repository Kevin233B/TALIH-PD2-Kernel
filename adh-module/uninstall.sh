#!/system/bin/sh
# uninstall.sh —— 清空内核态退场（reset+commit 发布空态 = 全可见/禁用）。
# /data/adb/adh 配置保留（用户数据，重装即恢复生效）。

MODDIR=${0%/*}
. "$MODDIR/common.sh"

if [ -e "$SYSFS" ]; then
    echo reset > "$SYSFS" 2>/dev/null
    echo commit > "$SYSFS" 2>/dev/null
    log "模块卸载：内核态已清空"
fi

pid=$(cat "$ADH_DIR/daemon.pid" 2>/dev/null)
[ -n "$pid" ] && kill "$pid" 2>/dev/null
exit 0
