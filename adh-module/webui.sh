#!/system/bin/sh
# webui.sh —— WebUI 后端：status 输出 JSON；save 写配置并即时发布。
#   status → {"module":{version},"kernel":{enabled,parents,rules,sys}|null,
#             "apps":[{pkg,uid,sys,hidden,visibleTo:[...]},...]}
#   save <hide_csv> <visible_csv>
#     hide_csv   逗号分隔的封堵目标包名
#     visible_csv 逗号分隔的“目标:授权方+授权方”条目
#   临时文件 + mv 原子替换（daemon 不会读到半写状态）。

MODDIR=${0%/*}
. "$MODDIR/common.sh"

case "$1" in
status)
    # 内核态首行：enabled %d parents %u rules %u sys %u
    k="null"
    if [ -r "$SYSFS" ]; then
        set -- $(head -n1 "$SYSFS" 2>/dev/null)
        if [ "$1" = "enabled" ]; then
            k="{\"enabled\":$2,\"parents\":$4,\"rules\":$6,\"sys\":$8}"
        fi
    fi
    ver=$(sed -n 's/^version=//p' "$MODDIR/module.prop" 2>/dev/null)
    printf '{"module":{"version":"%s"},"kernel":%s,"apps":[' "$ver" "$k"
    awk -v H="$HIDE_LIST" -v V="$VISIBLE_LIST" '
    BEGIN {
        while ((getline l < H) > 0) {
            sub(/[\r\n]/, "", l); sub(/[ \t].*/, "", l)
            if (l != "" && l !~ /^#/) hide[l] = 1
        }
        while ((getline l < V) > 0) {
            sub(/[\r\n]/, "", l); sub(/#.*/, "", l)
            n = split(l, a, /[ \t]+/)
            if (n > 1) for (i = 2; i <= n; i++) {
                # 注意：awk 的 a[k]=<引用 a[k] 的表达式> 会让 LHS 元素先于 RHS
                # 创建（in 判断恒真 → 前导逗号）——必须经临时变量中转
                v = (a[1] in vis) ? vis[a[1]] "," : ""
                vis[a[1]] = v "\"" a[i] "\""
            }
        }
    }
    NF && $1 !~ /^#/ {
        sys = ($NF == "@system") ? "true" : "false"
        h = ($1 in hide) ? "true" : "false"
        v = ($1 in vis) ? vis[$1] : ""
        printf "%s{\"pkg\":\"%s\",\"uid\":%d,\"sys\":%s,\"hidden\":%s,\"visibleTo\":[%s]}", \
            sep, $1, $2, sys, h, v
        sep = ","
    }' "$PKG_LIST" 2>/dev/null
    printf ']}\n'
    ;;

save)
    tmp_h=$(mktemp /data/local/tmp/.adh_h.XXXXXX)
    tmp_v=$(mktemp /data/local/tmp/.adh_v.XXXXXX)
    if [ -n "$2" ]; then
        echo "$2" | tr ',' '\n' > "$tmp_h"
    else
        : > "$tmp_h"
    fi
    if [ -n "$3" ]; then
        # CSV：目标:授权方+授权方，…… —— + 转 . 无关（包名字符集无 +）
        echo "$3" | tr ',' '\n' | sed 's/+/ /g' | awk -F: '
            NF >= 1 && $1 != "" {
                s = $1
                for (i = 2; i <= NF; i++) s = s " " $i
                print s
            }' > "$tmp_v"
    else
        : > "$tmp_v"
    fi
    mkdir -p "$ADH_DIR"
    mv -f "$tmp_h" "$HIDE_LIST"
    mv -f "$tmp_v" "$VISIBLE_LIST"
    # 配置落盘即成功：发布失败不报 FAIL（daemon ≤60s 自动重试收敛），
    # WebUI 保存后重载的内核状态横幅会显示真实生效情况
    if sh "$MODDIR/refresh.sh" >/dev/null 2>&1; then
        log "WebUI 保存并发布"
    else
        log "WebUI 保存（发布未完成——daemon 自动重试）"
    fi
    echo OK
    ;;

*)
    echo "usage: webui.sh status | webui.sh save <hide_csv> <visible_csv>"
    exit 1
    ;;
esac
