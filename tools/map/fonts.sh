#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
font=.cache/NotoSans.ttf
expected=bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d
if [[ ! -f "$font" ]]; then
  curl -fL 'https://raw.githubusercontent.com/google/fonts/main/ofl/notosans/NotoSans%5Bwdth,wght%5D.ttf' -o "$font"
fi
printf '%s  %s\n' "$expected" "$font" | shasum -a 256 --check
export PATH="$PWD/.cache/bun/bun-darwin-aarch64:$PATH"
cd tools/map
for size in 18 22 30; do
  bun x --bun lv_font_conv --font "../../$font" \
    -r '0x20-0x7F,0xB7,0x400-0x45F,0x490-0x491,0x2013-0x2014,0x2022' \
    --size "$size" --format lvgl --bpp 4 --no-compress \
    --lv-font-name "ui_font_$size" -o "../../generated/ui_font_$size.c"
done
printf 'FONTS_READY\n'
