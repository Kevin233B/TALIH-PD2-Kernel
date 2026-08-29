#!/usr/bin/env python3
# KMI 探测：比对「gki_defconfig 实际构建产出的 symvers」与「树内 android/abi_gki_aarch64.xml
# （本分支=google/android12-5.10.264 纯基线，7298 个冻结 KMI 符号）」，验证 google 树自证复现。
#
# 背景（rebase 定案，见 memory talih-pd2-kmi-subset-finding「Stage 0 实测定案」）：MTK 分叉在
# 核心头结构上落后 Google .264 一整段（sock.h 3 vs 74、fs.h 4 vs 49、skbuff.h 3 vs 35 …），
# genksyms CRC 编码 struct 字段布局 → MTK 源码编不出 Google 声明的 6401 CRC（crc_diff=609），
# 原地增量走不通，定案 rebase 到 google 纯基线。本脚本升级为「自证锚点」：google 树 + 标准链
# （gki_defconfig + LLVM_IAS=1 + clang-r416183b + FULL LTO）编出的 vmlinux 必须与树内自身
# abi_gki_aarch64.xml 的 7298 个冻结符号 CRC 全部一致（crc_diff=0）→ 证明这是真 GKI，
# 官方 DDK ko（module_layout=0x7c24b32d 两边相同）的静态依赖即可对齐。
#
# 关键：`make vmlinux` 只生成 vmlinux.symvers（scripts/link-vmlinux.sh 末尾
#   `${MAKE} -f scripts/Makefile.modpost MODPOST_VMLINUX=1` 产出 vmlinux 的导出符号+CRC）；
#   Module.symvers 要等 modules 阶段才汇总。故默认读 vmlinux.symvers，兜底 Module.symvers。
#
# 语义：
#   match    —— 声明 KMI 符号里，构建同样导出且 CRC 相同（目标=全部 6401）
#   crc_diff —— 同名但 CRC 不同（=真 KMI 破坏，目标=0，非零即 FAIL）
#   missing  —— 声明有、构建无（gki_defconfig 漏出；报前 20）
#   extra    —— 构建有、声明无（非冻结 GPL 导出，如 kallsyms_lookup_name，属正常）
#
# 用法：python3 kmi_probe.py [symvers 路径] [abi xml 路径]

import os
import re
import sys

ABI_XML = sys.argv[2] if len(sys.argv) > 2 else "android/abi_gki_aarch64.xml"
_DEFAULT_SYMVERS = ["out_gki/vmlinux.symvers", "out_gki/Module.symvers"]


def resolve_symvers():
    """优先命令行显式参数，其次 vmlinux.symvers，兜底 Module.symvers，全无则报清晰错误。"""
    cands = ([sys.argv[1]] if len(sys.argv) > 1 else []) + _DEFAULT_SYMVERS
    for p in cands:
        if os.path.isfile(p):
            return p
    print(f"!! 找不到 symvers 文件，检查过：{cands}")
    print("   make vmlinux 应产出 out_gki/vmlinux.symvers；若不存在，确认 gki_defconfig 构建确实成功。")
    sys.exit(2)


def load_abi(path):
    """从 abi xml 提取带 CRC 的 elf-symbol（单行 <elf-symbol name='X' ... crc='0xYY'/>）"""
    symbols = {}
    with open(path) as f:
        for line in f:
            m = re.search(r"name='([^']+)'.*?crc='([^']+)'", line)
            if m:
                symbols[m.group(1)] = int(m.group(2), 16)
    return symbols


def load_symvers(path):
    """symvers 每行：0xCRC<TAB>符号名<TAB>... 取前两列"""
    symbols = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                symbols[parts[1]] = int(parts[0], 16)
    return symbols


def main():
    symvers = resolve_symvers()
    abi = load_abi(ABI_XML)
    build = load_symvers(symvers)

    match = 0
    crc_diff = 0
    crc_diffs = []
    missing = []
    for name, crc in sorted(abi.items()):
        if name not in build:
            missing.append(name)
        elif build[name] != crc:
            crc_diff += 1
            crc_diffs.append(name)
        else:
            match += 1

    extra = sorted(set(build) - set(abi))

    print(f"[kmi_probe] symvers 来源    : {symvers}")
    print(f"[kmi_probe] abi 声明符号   : {len(abi)}")
    print(f"[kmi_probe] 构建导出符号   : {len(build)}")
    print(f"[kmi_probe] match(CRC 一致): {match}")
    print(f"[kmi_probe] crc_diff(破坏) : {crc_diff}")
    print(f"[kmi_probe] missing(漏出)  : {len(missing)}")
    if missing:
        print("  missing 前 20:", missing[:20])
    print(f"[kmi_probe] extra(非冻结GPL): {len(extra)}")

    # 全量打印 CRC diff（不限 10 个）——46 级别的漂移须看全列表才能聚类定位根因
    if crc_diffs:
        print(f"  [CRC DIFF] 全部 {len(crc_diffs)} 个：")
        for name in crc_diffs:
            print(f"  [CRC DIFF] {name}: abi=0x{abi[name]:08x} build=0x{build[name]:08x}")

    if crc_diff > 0:
        print("!! FAIL: 存在同名异 CRC 符号，构建与声明 KMI 不一致")
        sys.exit(1)
    if match == 0:
        print("!! FAIL: 无任何匹配，检查 gki_defconfig 构建是否成功 / symvers 是否生成")
        sys.exit(1)
    print(f"[kmi_probe] OK: 构建复现声明 KMI（{match}/{len(abi)} 符号 CRC 一致）")


if __name__ == "__main__":
    main()
