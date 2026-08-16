#!/bin/bash
set -eu

# 本地手动集成 SukiSU-Ultra builtin + SUSFS 2.2.0（与 CI 流程一致）。
# 用法: bash Integrate_SUSFS.sh

KERNEL_ROOT=$(pwd)

echo "[+] 清理旧的 KernelSU 集成"
rm -rf "$KERNEL_ROOT/KernelSU" "$KERNEL_ROOT/drivers/kernelsu"
git checkout -- drivers/Makefile drivers/Kconfig

echo "[+] 拉取 SukiSU-Ultra builtin 并接入 drivers/"
curl -LSs "https://raw.githubusercontent.com/SukiSU-Ultra/SukiSU-Ultra/main/kernel/setup.sh" | bash -s builtin

echo "[+] 应用 SukiSU-Ultra 4.19 兼容修复"
patch -p1 < "$KERNEL_ROOT/patch/10_fix_sukisu_4.19_compat.patch"

echo "[+] 应用 SUSFS 2.2.0 内核补丁 (non-GKI 4.19)"
patch -p1 < "$KERNEL_ROOT/patch/50_add_susfs_in_kernel-4.19.patch"

echo "[+] 完成。接下来运行 download_clang.sh && build-mt8797.sh"
