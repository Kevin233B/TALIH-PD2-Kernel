#!/system/bin/sh
# service.sh —— 开机入口：确保配置文件存在 → 首轮发布 → 拉起轮询守护。

MODDIR=${0%/*}
. "$MODDIR/common.sh"

mkdir -p "$ADH_DIR"
[ -f "$HIDE_LIST" ] || : > "$HIDE_LIST"
[ -f "$VISIBLE_LIST" ] || : > "$VISIBLE_LIST"

sh "$MODDIR/refresh.sh" >/dev/null 2>&1

# 防重复拉起（软重启场景）
old=$(cat "$ADH_DIR/daemon.pid" 2>/dev/null)
if [ -n "$old" ] && [ -d "/proc/$old" ]; then
    exit 0
fi
nohup sh "$MODDIR/daemon.sh" >/dev/null 2>&1 &
echo $! > "$ADH_DIR/daemon.pid"
