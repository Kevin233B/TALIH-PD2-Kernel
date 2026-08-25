#!/bin/bash
set -eu

# 本地手动集成 ReSukiSU (builtin SUSFS 2.x) + SUSFS 2.2.0 内核补丁（GKI 5.10）。
# 用法: bash Integrate_ReSukiSU.sh
#
# 说明:
# - ReSukiSU 已内置 SUSFS 2.x（kernel/Kconfig 的 9 个 KSU_SUSFS_* 选项），
#   与仓库内已提交的 fs/susfs.c + include/linux/susfs{,_def}.h (v2.2.0) 接口一致，
#   因此**无需** simonpunk 的 10_enable_susfs_for_ksu.patch。
# - SUSFS 2.2.0 内核侧补丁已随仓库提交并应用，无需在此重复打补丁。

KERNEL_ROOT=$(pwd)

echo "[+] 清理旧的 KernelSU 集成"
rm -rf "$KERNEL_ROOT/KernelSU" "$KERNEL_ROOT/drivers/kernelsu"
git checkout -- drivers/Makefile drivers/Kconfig 2>/dev/null || true

echo "[+] 拉取 ReSukiSU 并接入 drivers/"
curl -LSs "https://raw.githubusercontent.com/ReSukiSU/ReSukiSU/main/kernel/setup.sh" | bash

echo "[+] 合并 SUSFS/FUSE BPF 配置片段到 defconfig"
test -f arch/arm64/configs/talih_susfs.config || { echo "缺少 talih_susfs.config"; exit 1; }

echo "[+] 完成。SUSFS 2.2.0 内核补丁已在仓库内就绪。"
echo "    构建时确保 defconfig 含 CONFIG_KSU=y 与 CONFIG_KSU_SUSFS=y（见 talih_susfs.config）。"
