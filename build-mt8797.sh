#!/bin/bash
set -e
CLANG_PATH="$PWD/prebuilts/clang/host/linux-x86/clang-r383902"
CLANG_BIN="$CLANG_PATH/bin"
if [ ! -f "$CLANG_BIN/clang" ]; then
curl -L -o "$CLANG_BIN/clang" "https://github.com/Kevin233B/TALIH-PD2-Kernel/releases/download/other/clang"
chmod +x "$CLANG_BIN/clang"
fi
curl -LSs "https://raw.githubusercontent.com/SukiSU-Ultra/SukiSU-Ultra/main/kernel/setup.sh" | bash -s builtin
make O=out ARCH=arm64 CC=$CLANG_BIN/clang LD=$CLANG_BIN/ld.lld AR=$CLANG_BIN/llvm-ar NM=$CLANG_BIN/llvm-nm OBJCOPY=$CLANG_BIN/llvm-objcopy OBJDUMP=$CLANG_BIN/llvm-objdump READELF=$CLANG_BIN/llvm-readelf OBJSIZE=$CLANG_BIN/llvm-size STRIP=$CLANG_BIN/llvm-strip CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 LLVM_IAS=1 ls12_mt8797_wifi_64_defconfig
make O=out ARCH=arm64 CC=$CLANG_BIN/clang LD=$CLANG_BIN/ld.lld AR=$CLANG_BIN/llvm-ar NM=$CLANG_BIN/llvm-nm OBJCOPY=$CLANG_BIN/llvm-objcopy OBJDUMP=$CLANG_BIN/llvm-objdump READELF=$CLANG_BIN/llvm-readelf OBJSIZE=$CLANG_BIN/llvm-size STRIP=$CLANG_BIN/llvm-strip CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 LLVM_IAS=1 -j$(nproc)
