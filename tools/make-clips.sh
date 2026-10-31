#!/usr/bin/env bash
# Test clips for tools/bench.sh, cut from a real ShadowPlay/OBS recording (all audio tracks kept):
#   .out/clips/<name>-1080p.mp4   lossless stream-copy cut of the source
#   .out/clips/<name>-4k60.mp4    the same cut upscaled to 3840x2160 (HEVC, NVENC when available), audio copied
# Usage: bash tools/make-clips.sh <source video> [start seconds, default 30] [length seconds, default 30]
# Needs ffmpeg on PATH. Existing outputs are kept; delete them to rebuild.
set -euo pipefail
cd "$(dirname "$0")/.."
src="${1:?usage: bash tools/make-clips.sh <source video> [start] [length]}"
start="${2:-30}"; len="${3:-30}"
name="$(basename "${src%.*}" | tr -c 'A-Za-z0-9.\n-' '_')"
out=.out/clips; mkdir -p "$out"

cut="$out/$name-1080p.mp4"
[ -f "$cut" ] || ffmpeg -v error -ss "$start" -t "$len" -i "$src" -map 0 -c copy "$cut"

uhd="$out/$name-4k60.mp4"
if [ ! -f "$uhd" ]; then
  if ffmpeg -v error -hide_banner -f lavfi -i nullsrc=s=256x256 -frames:v 1 -c:v hevc_nvenc -f null - 2>/dev/null; then
    venc=(-c:v hevc_nvenc -preset p5 -cq 20)
  else
    venc=(-c:v libx265 -preset fast -crf 20)
  fi
  ffmpeg -v error -i "$cut" -map 0:v:0 -map 0:a -vf scale=3840:2160:flags=lanczos "${venc[@]}" -c:a copy "$uhd"
fi

for f in "$cut" "$uhd"; do
  printf '%s  ' "$f"
  ffprobe -v error -show_entries stream=codec_name,width,height,r_frame_rate -of csv=p=0 "$f" | tr '\n' ' '
  echo
done
