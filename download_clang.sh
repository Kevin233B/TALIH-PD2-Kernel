CLANG_PATH="$PWD/prebuilts/clang/host/linux-x86/clang-r383902"
CLANG_BIN="$CLANG_PATH/bin"
if [ ! -f "$CLANG_BIN/clang" ] || [ "$(head -c 4 "$CLANG_BIN/clang" 2>/dev/null)" != "$(printf '\x7fELF')" ]; then
curl -L --retry 8 --retry-delay 5 --retry-all-errors \
     -H "User-Agent: Mozilla/5.0 (X11; Linux x86_64)" \
     -o "$CLANG_BIN/clang" \
     "https://github.com/Kevin233B/TALIH-PD2-Kernel/releases/download/other/clang"
chmod +x "$CLANG_BIN/clang"
fi
if [ "$(head -c 4 "$CLANG_BIN/clang" 2>/dev/null)" != "$(printf '\x7fELF')" ]; then
echo "错误: clang 下载失败(非 ELF 文件), 请检查网络或手动放置 clang 到 $CLANG_BIN/" >&2
exit 1
fi
