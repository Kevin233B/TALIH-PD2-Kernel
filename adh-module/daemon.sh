#!/system/bin/sh
# daemon.sh —— 轮询守护（10s 周期，变更即重发布）：
#   - hide.list / visible.list 手工或 WebUI 修改
#   - packages.list 变化（应用装卸——覆盖 uid 复用与名单内目标重装）
#   - pm 首次就绪补发布（service.sh 首轮可能早于 pm 完全启动，
#     被商店更新过的系统应用须待 pm 补进放行集）
# 失败退避：refresh 硬错误写 .fail 时间戳——60 秒内不重试（防 ENOSPC 类
# 配置超限无限刷日志），配置文件在 .fail 之后被再改则立即重试。
# 开机首轮可能早于 emulated storage/SELinux 标签就绪（kern_path 失败属
# 时序性），60s 退避保证收敛且不刷日志。
# refresh 自带 mkdir 原子锁，与 WebUI 保存并发安全。

MODDIR=${0%/*}
. "$MODDIR/common.sh"

echo $$ > "$ADH_DIR/daemon.pid"

while true; do
    sleep 10
    [ -e "$SYSFS" ] || continue

    if [ ! -f "$PMREADY" ]; then
        if [ -n "$(/system/bin/pm list packages -s -U 2>/dev/null | head -n1)" ]; then
            sh "$MODDIR/refresh.sh" >/dev/null 2>&1
        fi
        continue
    fi

    if [ ! -f "$STAMP" ] || [ "$HIDE_LIST" -nt "$STAMP" ] || \
       [ "$VISIBLE_LIST" -nt "$STAMP" ] || [ "$PKG_LIST" -nt "$STAMP" ]; then
        if [ -f "$FAIL" ] && \
           [ "$(( $(date +%s) - $(cat "$FAIL" 2>/dev/null || echo 0) ))" -lt 60 ] && \
           [ ! "$HIDE_LIST" -nt "$FAIL" ] && [ ! "$VISIBLE_LIST" -nt "$FAIL" ]; then
            continue
        fi
        sh "$MODDIR/refresh.sh" >/dev/null 2>&1
    fi
done
