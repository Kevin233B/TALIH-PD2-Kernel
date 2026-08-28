#!/usr/bin/env python3
# 生成合法「空」DTBO 镜像，刷到 5.10 设备的 dtbo_a / dtbo_b 分区，
# 让 MTK lk 的 overlay 阶段变成 no-op（主 dtb 原样启动，不叠加 4.19 工厂 overlay）。
#
# 必须严格参照 lk 的 dtbo 解析逻辑（本设备 lk = app/mt_boot/odm_mdtbo.c 的
# load_dtb_from_dtb_entry + load_overlay_dtbo），否则会触发 assert(0) 直接砖：
#
#   1. header_size  != sizeof(struct dt_table_header) == 32            -> assert(0)
#   2. dt_entry_size != sizeof(struct dt_table_entry) == 32            -> assert(0)
#   3. magic 既非 MKIMG 也非 DT_TABLE(0xd7b7ab1e)                      -> "non of any known dtbo format" assert(0)
#   4. entry 指向的 payload 不是合法 FDT(magic 0xd00dfeed) 或
#      fdt_totalsize != entry.dt_size                                  -> assert(0)
#
# 因此 count=0 的空表 / 全 0 / 全 0xff 都是砖方案：lk 在 count 不匹配时 fallback 到
# entry[0]，仍会 malloc(dt_size) + partition_read + 校验 FDT_MAGIC，读不到合法 FDT 就 assert。
#
# 唯一两条 lk 路径（fdt_op.c 宽松 / odm_mdtbo.c 严格）都 100% 安全的形态：
#   合法 dt_table + count=1 + 内嵌 1 个「只有根节点、无 fragment」的空 FDT。
#   ufdt_apply_overlay 遍历 0 个 fragment -> 返回主 dtb 原样，等于不叠加。
#
# 与工厂 dtbo（assets/dtbo_a.img）逐字段一致：count=1、id=0、rev=0、custom 全 0，
# 唯一差异是 payload 换成 72 字节空 FDT。

import struct
import sys

# --- 空 FDT：72 字节，d00dfeed 合法最小设备树（只有根节点 /，无 fragment）---
# 由 `dtc -I dts -O dtb` 编译 `/dts-v1/; / { };` 所得，字节固定。
EMPTY_FDT = bytes.fromhex(
    "d00dfeed"  # fdt_header.magic
    "00000048"  # totalsize = 72
    "00000038"  # off_dt_struct = 56
    "00000048"  # off_dt_strings = 72（空字符串表）
    "00000028"  # off_mem_rsvmap = 40
    "00000011"  # version = 17
    "00000010"  # last_comp_version = 16
    "00000000"  # boot_cpuid_phys = 0
    "00000000"  # size_dt_strings = 0
    "00000010"  # size_dt_struct = 16
    "00000000"  # mem_rsvmap: addr_hi
    "00000000"  # mem_rsvmap: addr_lo
    "00000000"  # mem_rsvmap: size_hi
    "00000000"  # mem_rsvmap: size_lo（空保留映射结束）
    "00000001"  # FDT_BEGIN_NODE
    "00000000"  # 根节点名 ""（空，4 字节对齐）
    "00000002"  # FDT_END_NODE
    "00000009"  # FDT_END
)

# --- dt_table_header（大端，24 字节），字段值全部命中 lk 严格校验 ---
DT_TABLE_MAGIC = 0xD7B7AB1E
ENTRY_SIZE = 32          # sizeof(struct dt_table_entry)
HEADER_SIZE = 32         # sizeof(struct dt_table_header)
COUNT = 1                # 必须 >=1，fallback 逻辑才能选到一个合法 FDT
ENTRIES_OFFSET = 32
PAGE_SIZE = 0x800
VERSION = 0

# --- dt_table_entry（大端，32 字节）---
entry_dt_size = len(EMPTY_FDT)
entry_dt_offset = HEADER_SIZE + ENTRY_SIZE  # 0x40，payload 紧跟 table+entry
entry_id = 0
entry_rev = 0
entry_custom = (0, 0, 0, 0)

def main(out_path):
    header = struct.pack(
        ">8I",
        DT_TABLE_MAGIC,
        HEADER_SIZE + ENTRY_SIZE + entry_dt_size,  # total_size
        HEADER_SIZE,
        ENTRY_SIZE,
        COUNT,
        ENTRIES_OFFSET,
        PAGE_SIZE,
        VERSION,
    )
    entry = struct.pack(
        ">8I",
        entry_dt_size,
        entry_dt_offset,
        entry_id,
        entry_rev,
        *entry_custom,
    )
    img = header + entry + EMPTY_FDT
    with open(out_path, "wb") as f:
        f.write(img)
    print(f"[gen_empty_dtbo] wrote {len(img)} bytes -> {out_path}")
    print(f"  magic=0x{DT_TABLE_MAGIC:08x} header_size={HEADER_SIZE} "
          f"entry_size={ENTRY_SIZE} count={COUNT} -> payload FDT {entry_dt_size}B (valid, no fragment)")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "empty_dtbo.img")
