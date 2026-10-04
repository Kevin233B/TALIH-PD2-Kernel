#!/system/bin/sh
# common.sh —— adh 模块共享定义与函数（refresh/daemon/service/webui 引用）

ADH_DIR=/data/adb/adh
HIDE_LIST=$ADH_DIR/hide.list
VISIBLE_LIST=$ADH_DIR/visible.list
STAMP=$ADH_DIR/.stamp
LOCK=$ADH_DIR/.lock
PMREADY=$ADH_DIR/.pmready
PKG_LIST=/data/system/packages.list
SYSFS=/sys/kernel/android_data_hide/rules

# 受管父目录（bind 多视图同 inode，内核按 (dev,ino) 去重）
PARENTS="/data/data /data/user/0 /data/user_de/0 /data/media/0/Android/data /data/media/0/Android/obb"

log() { echo "$(date '+%F %T') $*" >> "$ADH_DIR/adh.log"; }

# 包名 → uid（未安装则输出空）：
# 权威源 packages.list（明文，随 pm 实时更新，开机任意时刻可读）；
# 数据目录属主 stat 兜底（device-protected 等边缘布局）。
resolve_uid() {
    local u d
    u=$(awk -v p="$1" '$1==p {print $2; exit}' "$PKG_LIST" 2>/dev/null)
    if [ -n "$u" ]; then
        echo "$u"
        return
    fi
    for d in /data/data/$1 /data/user/0/$1 /data/user_de/0/$1; do
        if [ -d "$d" ]; then
            stat -c %u "$d"
            return
        fi
    done
}

# 配置文件 → 有效包名清单（跳过空行与 # 注释行）
list_pkgs() {
    awk 'NF && $1 !~ /^#/ {print $1}' "$1" 2>/dev/null
}

# 目标包 → 该目标的可见授权调用方清单（visible.list 行格式：目标 调用方 调用方…）
visible_callers_for() {
    awk -v t="$1" 'NF && $1 !~ /^#/ && $1==t {for (i=2; i<=NF; i++) print $i}' \
        "$VISIBLE_LIST" 2>/dev/null
}
