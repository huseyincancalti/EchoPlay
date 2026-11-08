#!/usr/bin/env bash
# Picture-quality check: the same paused frame rendered by the v1 engine path (gpu-next, d3d11) and by the
# v2 spike (libmpv render API), both in a 1280x720 logical window with v1's scalers. Saves both PNGs and
# prints SSIM/PSNR between them (SSIM 1.0 = identical).
# Usage: bash tools/shot-compare.sh <video> <seconds>     Output: .out/shots/<name>-{v1,v2}.png
set -euo pipefail
cd "$(dirname "$0")/.."
. .out/devenv.sh
video="${1:?usage: bash tools/shot-compare.sh <video> <seconds>}"; at="${2:?seconds}"
name="$(basename "${video%.*}" | tr -c 'A-Za-z0-9.\n-' '_')-t$at"
out=.out/shots; mkdir -p "$out"
a="$out/$name-v1.png"; b="$out/$name-v2.png"; rm -f "$a" "$b"

# timeout: a file that does not open would leave either player waiting for its first frame for good.
timeout 90 "$MPV_EXE" --no-config --vo=gpu-next --gpu-api=d3d11 --hwdec=auto-safe \
  --scale=lanczos --cscale=lanczos --dscale=mitchell --geometry="$V1_GEOMETRY" --mute=yes \
  --start="$at" --pause=yes --screenshot-format=png --script=tools/bench/v1-probe.lua \
  --script-opts=shot-out="$(cygpath -m "$PWD/$a")" "$video" >/dev/null 2>&1 || true
timeout 90 .out/build-v2/EchoPlay.exe --no-glass --shot-at "$at" --shot-out "$(cygpath -m "$PWD/$b")" "$video" >/dev/null 2>&1 || true

for f in "$a" "$b"; do
  [ -f "$f" ] || { echo "missing $f"; exit 1; }
  printf '%s  %s\n' "$f" "$(ffprobe -v error -show_entries stream=width,height -of csv=p=0 "$f")"
done

ffmpeg -hide_banner -i "$a" -i "$b" -lavfi "[0]split[a1][a2];[1]split[b1][b2];[a1][b1]ssim;[a2][b2]psnr" -f null - 2>&1 \
  | grep -oE "(SSIM|PSNR) .*" | sed -E 's/^(SSIM|PSNR)/  \1/'
