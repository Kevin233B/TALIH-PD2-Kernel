#!/usr/bin/env python3
"""ls12 5.10 boot.img 本地打包脚本（ramdisk 在 assets/ 不进 repo → 本地跑）。

工具链原则（用户定案）：**只用 AOSP 官方 mkbootimg 组件**，忽略 /root/kernel
的 4.19 老 mkbootimg 二进制。本脚本首次运行会用 urllib 从
android.googlesource.com 拉官方 `mkbootimg.py`（?format=TEXT base64 → 解码，
字节级精确）落到 .github/scripts/，之后离线复用。

Stage4 终点形态工具 = 同组件的 `gki/retrofit_gki.sh`（仅 android13-release
分支有，main 已删）——把认证 GKI boot + init_boot + vendor_boot 回退合并成
单 v2 boot.img；android12 无 init_boot，通用 ramdisk 取自 boot.img。本脚本
--retrofit 模式同样自取该官方脚本。参数依据见 memory talih-pd2-flash-guide
（真机 boot_a.img header v2 硬解析 + lk 反汇编定案）：
  base=0x40000000, kernel_offset=0x80000 (0x40080000),
  ramdisk_offset=0x11100000 (0x51100000), tags_offset=0x7c80000 (0x47c80000),
  page=2048, os_version=0x18000176,
  cmdline='bootopt=64S3,32N2,64N2 buildvariant=user'

用法（repo 根目录；.gz 件来自 CI artifact 的 Image.nobtf.gz，或裸 Image 也可）：
  python3 .github/scripts/package_bootimg.py \
      --kernel out_device/arch/arm64/boot/Image.nobtf.gz \
      --dtb out_device/arch/arm64/boot/dts/mediatek/ls12_mt8797_wifi_64.dtb \
      --ramdisk assets/boot_a/ramdisk.cpio.gz
  产物 boot.img + 尺寸预算校验（boot 分区 64MiB 硬约束）。
"""
import argparse
import base64
import gzip
import os
import shutil
import subprocess
import sys
import urllib.request

TOOLS = {
    # 官方 mkbootimg.py（system/tools/mkbootimg，main 分支）
    "mkbootimg.py":
        "https://android.googlesource.com/platform/system/tools/mkbootimg/"
        "+/refs/heads/main/mkbootimg.py?format=TEXT",
    # 官方 retrofit_gki.sh（仅 android13-release 分支存在，main 已删）
    "retrofit_gki.sh":
        "https://android.googlesource.com/platform/system/tools/mkbootimg/"
        "+/refs/heads/android13-release/gki/retrofit_gki.sh?format=TEXT",
}

RAMDISK_SIZE = 12239359      # 原厂 ramdisk.cpio.gz 实测
BOOT_PART = 64 * 1024 * 1024
PAGE = 2048


def pages(n: int) -> int:
    return (n + PAGE - 1) // PAGE * PAGE


def ensure_tool(name: str) -> str:
    """.github/scripts/<name> 不在则从 AOSP 官方源拉（base64 精确解码）。"""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), name)
    if not os.path.exists(path):
        url = TOOLS[name]
        print(f"拉取官方工具 {name} …")
        with urllib.request.urlopen(url, timeout=60) as r:
            raw = base64.b64decode(r.read())
        with open(path, "wb") as f:
            f.write(raw)
        print(f"  → {path}（{len(raw)} bytes）")
    return path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--kernel", required=True, help="裸 Image（CI 产物，会自动 gzip -9）")
    ap.add_argument("--dtb", required=True, help="5.10 编译的 ls12 dtb（禁用真机 4.19 dtb）")
    ap.add_argument("--ramdisk", default="assets/boot_a/ramdisk.cpio.gz")
    ap.add_argument("--out", default="boot.img")
    args = ap.parse_args()

    for p in (args.kernel, args.dtb, args.ramdisk):
        if not os.path.exists(p):
            print(f"缺文件: {p}", file=sys.stderr)
            return 1
    mkbootimg = ensure_tool("mkbootimg.py")

    # kernel 段 = gzip 的 Image（lk/kernel 自解压链认定 gzip）。
    # CI Size attribution step 已产出 Image.nobtf.gz → .gz 输入直接用（幂等不重压，
    # 本地打包从压 234MB 的几分钟缩到秒级）；裸 Image 输入则现场 gzip -9。
    if args.kernel.endswith(".gz"):
        kernel_gz = args.kernel
    else:
        kernel_gz = "Image.gz"
        with open(args.kernel, "rb") as fin, gzip.open(kernel_gz, "wb", compresslevel=9) as fout:
            shutil.copyfileobj(fin, fout)
    ksz = os.path.getsize(kernel_gz)
    dsz = os.path.getsize(args.dtb)
    rsz = os.path.getsize(args.ramdisk)
    total = PAGE + pages(ksz) + pages(rsz) + pages(dsz)

    print(f"kernel(gz)={ksz/1048576:.2f}MiB ramdisk={rsz/1048576:.2f}MiB dtb={dsz/1048576:.2f}MiB")
    print(f"boot.img 预估 = {total/1048576:.2f}MiB / 64MiB 分区，余量 {(BOOT_PART-total)/1048576:+.2f}MiB")
    if total > BOOT_PART:
        print("!! 超 64MiB 硬约束——先走瘦身刀（BTF 零填充 / Stage4 =m）再打包", file=sys.stderr)
        return 1

    cmd = [
        sys.executable, mkbootimg,
        "--header_version", "2",
        "--pagesize", str(PAGE),
        "--base", "0x40000000",
        "--kernel_offset", "0x80000",
        "--ramdisk_offset", "0x11100000",
        "--tags_offset", "0x7c80000",
        "--os_version", "0x18000176",
        "--cmdline", "bootopt=64S3,32N2,64N2 buildvariant=user",
        "--kernel", kernel_gz,
        "--ramdisk", args.ramdisk,
        "--dtb", args.dtb,
        "--output", args.out,
    ]
    print("+", " ".join(cmd))
    if subprocess.call(cmd) != 0:
        return 1
    actual = os.path.getsize(args.out)
    print(f"OK: {args.out} = {actual/1048576:.2f}MiB（{'✓ 分区内' if actual <= BOOT_PART else '✗ 超分区!'}）")
    print("刷入：fastboot flash boot_a boot.img && fastboot flash dtbo_a empty_dtbo.img")
    return 0 if actual <= BOOT_PART else 1


if __name__ == "__main__":
    sys.exit(main())
