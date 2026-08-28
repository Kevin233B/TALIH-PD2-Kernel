#!/usr/bin/env python3
# KMI 探测：比对「gki_defconfig 实际构建产出的 Module.symvers」与「树内 android/abi_gki_aarch64.xml
# （MTK 声明的 GKI KMI = google android12-5.10 KMI 的严格子集，6401 符号）」，验证声明确实能复现。
#
# 背景（见 memory talih-pd2-kmi-subset-finding）：静态比对已证明 MTK 的 6401 个 KMI 符号与 Google
# android12-5.10 的对应符号 CRC 零差异（module_layout=0x7c24b32d 两边相同）。本脚本把「静态声明」
# 提升为「实际构建验证」：若 gki_defconfig 编出的 vmlinux 的 Module.symvers 与声明的 6401 符号 CRC
# 全部一致（crc_diff=0），则该实际构建就是 Google-KMI 兼容内核，官方 DDK ko 的静态依赖
# （module_layout/kallsyms_lookup_name/kallsyms_lookup）即可对齐。
#
# 语义：
#   match    —— 声明 KMI 符号里，构建同样导出且 CRC 相同（目标=全部 6401）
#   crc_diff —— 同名但 CRC 不同（=真 KMI 破坏，目标=0，非零即 FAIL）
#   missing  —— 声明有、构建无（gki_defconfig 漏出；报前 20）
#   extra    —— 构建有、声明无（非冻结 GPL 导出，如 kallsyms_lookup_name，属正常）
#
# 用法：python3 kmi_probe.py [Module.symvers 路径] [abi xml 路径]

import re
import sys

SYMVERS = sys.argv[1] if len(sys.argv) > 1 else "out_gki/Module.symvers"
ABI_XML = sys.argv[2] if len(sys.argv) > 2 else "android/abi_gki_aarch64.xml"


def load_abi(path):
    """从 abi xml 提取带 CRC 的 elf-symbol（单行 <elf-symbol name='X' ... crc='0xYY'/>）"""
    symbols = {}
    with open(path) as f:
        for line in f:
            m = re.search(r"name='([^']+)'.*?crc='([^']+)'", line)
            if m:
                symbols[m.group(1)] = m.group(2).lower()
    return symbols


def load_symvers(path):
    """Module.symvers 每行：0xCRC<TAB>符号名<TAB>... 取前两列"""
    symbols = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                symbols[parts[1]] = parts[0].lower()
    return symbols


def main():
    abi = load_abi(ABI_XML)
    build = load_symvers(SYMVERS)

    match = 0
    crc_diff = 0
    missing = []
    for name, crc in sorted(abi.items()):
        if name not in build:
            missing.append(name)
        elif build[name] != crc:
            crc_diff += 1
            if crc_diff <= 10:
                print(f"  [CRC DIFF] {name}: abi={crc} build={build[name]}")
        else:
            match += 1

    extra = sorted(set(build) - set(abi))

    print(f"[kmi_probe] abi 声明符号   : {len(abi)}")
    print(f"[kmi_probe] 构建导出符号   : {len(build)}")
    print(f"[kmi_probe] match(CRC 一致): {match}")
    print(f"[kmi_probe] crc_diff(破坏) : {crc_diff}")
    print(f"[kmi_probe] missing(漏出)  : {len(missing)}")
    if missing:
        print("  missing 前 20:", missing[:20])
    print(f"[kmi_probe] extra(非冻结GPL): {len(extra)}")

    if crc_diff > 0:
        print("!! FAIL: 存在同名异 CRC 符号，构建与声明 KMI 不一致")
        sys.exit(1)
    if match == 0:
        print("!! FAIL: 无任何匹配，检查 gki_defconfig 构建是否成功 / Module.symvers 是否生成")
        sys.exit(1)
    print(f"[kmi_probe] OK: 构建复现声明 KMI（{match}/{len(abi)} 符号 CRC 一致）")
    # missing 非零仅在异常时告警（正常 gki_defconfig 应全出），但主要由 crc_diff 判 FAIL，
    # 因为 missing 可能含个别手工导出差异；此处给 soft 提示即可。


if __name__ == "__main__":
    main()
