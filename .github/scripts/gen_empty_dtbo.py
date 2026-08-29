#!/usr/bin/env python3
"""生成「空」dtbo 镜像（ls12 / MT6893，5.10 刷机专用）。

背景（lk 反汇编定案，见 memory talih-pd2-flash-guide）：
  - 5.10 dtb 是完整重构版（节点/phandle 重编号），绝不能再叠 4.19 工厂 overlay
    （81 个 fragment 全是 <0xffffffff> 占位 + __fixups__ 符号重定位，跨版必错）。
  - 但本设备 lk（app/mt_boot/odm_mdtbo.c 的 load_dtb_from_dtb_entry）有 4 个
    assert(0) 硬砖点：header_size!=32 / entry_size!=32 / magic 不认 / entry
    payload 非合法 FDT 或 fdt_totalsize != dt_size。
  - 所以「count=0 空表 / 全 0 / 全 0xff」全都会砖；唯一安全形态 =
    合法 AOSP dt_table（大端）+ count=1 + 内嵌合法「空 FDT」（仅根节点，
    0 个 fragment）→ ufdt_apply_overlay 遍历 0 次 = no-op，主 dtb 原样。

AOSP DT_TABLE_MAGIC = 0xd7b7ab1e（真机 dtbo_a.img 实测字节 d7 b7 ab 1e 大端）。

产物 136 字节：32 表头 + 32 entry + 72 空 FDT。
用法：python3 gen_empty_dtbo.py <output.img>
"""
import struct
import sys

DT_TABLE_MAGIC = 0xD7B7AB1E
FDT_MAGIC = 0xD00DFEED
FDT_BEGIN_NODE = 0x1
FDT_END_NODE = 0x2
FDT_END = 0x9


def build_empty_fdt() -> bytes:
    """最小合法空 FDT：header(40) + 空 reserve map(16) + 根节点 struct(16) = 72B。"""
    header_size = 40
    off_mem_rsvmap = header_size            # 40：紧跟 header
    mem_rsvmap = struct.pack(">QQ", 0, 0)   # 一对 (0,0) 终止符 = 16B
    off_dt_struct = off_mem_rsvmap + len(mem_rsvmap)          # 56
    # 根节点 ""（含 \0 恰好 4B 对齐）+ END_NODE + END
    dt_struct = struct.pack(">I", FDT_BEGIN_NODE) + b"\x00\x00\x00\x00"
    dt_struct += struct.pack(">II", FDT_END_NODE, FDT_END)    # 16B
    total = off_dt_struct + len(dt_struct)   # 72
    off_dt_strings = total                    # 无 strings 区
    header = struct.pack(
        ">10I",
        FDT_MAGIC,        # magic
        total,            # totalsize = 72
        off_dt_struct,    # off_dt_struct = 56
        off_dt_strings,   # off_dt_strings = 72
        off_mem_rsvmap,   # off_mem_rsvmap = 40
        17,               # version
        16,               # last_comp_version
        0,                # boot_cpuid_phys
        0,                # size_dt_strings
        len(dt_struct),   # size_dt_struct = 16
    )
    blob = header + mem_rsvmap + dt_struct
    assert len(blob) == 72, len(blob)
    return blob


def build_empty_dtbo() -> bytes:
    fdt = build_empty_fdt()
    header_size = 32
    entry_size = 32
    entry_offset = header_size
    total = header_size + entry_size + len(fdt)   # 32+32+72 = 136
    header = struct.pack(
        ">8I",
        DT_TABLE_MAGIC,   # magic
        total,            # total_size
        header_size,      # header_size = 32（lk assert 点①）
        entry_size,       # dt_entry_size = 32（lk assert 点②）
        1,                # dt_entry_count = 1（count=0 会走 entry[0] fallback，勿用）
        entry_offset,     # dt_entries_offset = 32
        2048,             # page_size（照真机 dtbo_a）
        0,                # version
    )
    entry = struct.pack(
        ">8I",
        len(fdt),         # dt_size = 72（lk 校验 fdt_totalsize == dt_size）
        entry_offset + entry_size,  # dt_offset = 64
        0, 0,             # id / rev
        0, 0, 0, 0,       # custom[4]
    )
    blob = header + entry + fdt
    assert len(blob) == 136, len(blob)
    return blob


def main() -> int:
    out = sys.argv[1] if len(sys.argv) > 1 else "empty_dtbo.img"
    blob = build_empty_dtbo()
    with open(out, "wb") as f:
        f.write(blob)
    print(f"{out}: {len(blob)} bytes")
    # 自检：回读校验 lk 的四个 assert 点
    with open(out, "rb") as f:
        data = f.read()
    magic, total, hs, es, cnt, eo, _, _ = struct.unpack(">8I", data[:32])
    dt_size, dt_off = struct.unpack(">2I", data[32:40])
    fdt_magic, fdt_total = struct.unpack(">2I", data[dt_off:dt_off + 8])
    assert magic == DT_TABLE_MAGIC and hs == 32 and es == 32 and cnt == 1
    assert fdt_magic == FDT_MAGIC and fdt_total == dt_size == 72
    assert total == len(data) == 136
    print("self-check OK: dt_table 合法 + 空 FDT(72B) + lk 4 assert 点全过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
