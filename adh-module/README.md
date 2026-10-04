# android_data_hide —— 应用存在性封堵（adh）

按**目录属主 inode uid** 键控，封堵检测应用经
`/storage/emulated/<n>/Android/{data,obb}/<包名>`、`/data/data/<包名>`、
`/data/user/<n>/<包名>`、`/data/user_de/<n>/<包名>` 探测应用存在性。

## 前置

配套内核：`CONFIG_ANDROID_DATA_HIDE=y`（本仓 `susfs+resukisu` 分支默认开启），
配置面 `/sys/kernel/android_data_hide/rules`。

## 语义

对隐藏名单内的目标，非属主普通应用（uid ≥ 10000，含多用户/isolated/
app-zygote-isolated）得到与“该应用未安装”逐字节一致的回答：

| 探测方式 | 回答 | 与真不存在的对照 |
|---|---|---|
| stat / faccessat / open / exec / 深路径 / 符号链接穿越 | `ENOENT` | 一致 |
| mkdir / rename 目标 / mknod / symlink / link（create 类末位） | `EACCES` | 一致（真不存在 + 父目录无写权限同构） |
| unlink / rmdir / rename 源端 | `ENOENT` | 一致 |
| `O_CREAT\|O_EXCL` | `EACCES`（不泄露 EEXIST） | 一致 |

完全放行：属主自身、uid < 10000（root/system/shell/media 等一条比较全覆盖）、
预装系统应用（uid ≥ 10000 者自动进放行集）、用户对单个目标授权可见的应用。

键控按属主 uid 而非路径名：大小写/casefold 变体、零宽码点变体、应用卸载重装
（inode 重建但 uid 不变）全部自然覆盖；拒绝只作用于本次路径解析，
**零 dcache 写入**（跨 uid 共享 dentry 树无污染）、**零挂载拓扑变化**
（statfs/mountinfo 逐字节不变）、无 SELinux avc 审计痕迹。

## 配置

- KSU WebUI（Miuix 风格）：应用列表开关封堵、单个目标的可见授权编辑
- `/data/adb/adh/hide.list`：每行一个封堵目标包名
- `/data/adb/adh/visible.list`：每行 `目标包名 授权可见的调用方包名…`
- `#` 开头为注释；两文件手工修改后 ≤10 秒内由守护自动重发布

## 行为链

```
service.sh（开机）──▶ refresh.sh ──▶ /sys/kernel/android_data_hide/rules
                        ▲                 （reset → parent×5 → hide… → allow… → commit）
daemon.sh（10s 轮询）──┘        解析源：/data/system/packages.list（明文）+ pm 补充
```

- uid 权威源：`packages.list`（明文、开机任意时刻可读），数据目录 stat 兜底
- 系统放行集：`packages.list` 的 `@system` 行 ∪ `pm list packages -s -U`
  （后者覆盖被商店更新过的系统应用），uid ≥ 10000 者入集
- 应用装卸 ≤10 秒自动收敛（覆盖 uid 复用与名单内目标重装）
- `uninstall.sh`：`reset`+`commit` 发布空态 = 全可见；配置文件保留（用户数据）

## WebUI 构建

`webui/` 为源码（Vue 3 + miuix-vue），CI（`.github/workflows/build-adh-module.yml`）
构建产物为模块 zip（`webroot/` 内为编译后前端），直接在 KernelSU 安装。
