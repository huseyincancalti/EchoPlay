#!/usr/bin/env bash
# Pictures for a release page: the mixer as an animated GIF (game and mic switched live) on a recording, and stills
# of the panels over a film. It opens player windows on the screen for ~25 s (input goes to them only).
# Usage: bash tools/promo.sh <recording with game + mic tracks> <film>
# Output: .out/promo/out/mixer.gif and promo-*.png. Needs: bash tools/check.sh v2 and bash tools/e2e.sh once (its clip).
cd "$(dirname "$0")/.." || exit 3
. .out/devenv.sh || exit 3
[ -f "$1" ] && [ -f "$2" ] || { echo "usage: bash tools/promo.sh <recording> <film>"; exit 3; }
out=.out/promo/out
rm -rf "$out" && mkdir -p "$out"
ECHOPLAY_PROMO_CLIP="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")" ECHOPLAY_PROMO_FILM="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")" \
  ECHOPLAY_E2E_CLIP="$(pwd)/.out/e2e/two-tracks-60s.mp4" ECHOPLAY_E2E_SHOTS="$(pwd)/$out" ECHOPLAY_E2E_DIR="$(pwd)/.out/e2e/run" \
  .out/build-v2/tst_e2e.exe promo -o .out/promo/result.txt,txt
code=$?
grep -E "^(PASS|FAIL!|SKIP|QINFO)|^ +Loc|^Totals" .out/promo/result.txt
[ $code -eq 0 ] || { echo "PROMO: FAILED"; exit 1; }
# The test grabs at ~13 frames a second (a grab takes ~75 ms). An MP4 for the release page (GitHub plays it inline,
# 2-3 MB) and a 640 wide GIF under 10 MB for the README.
ffmpeg -v error -y -framerate 12.8 -i "$out/frames/%04d.png" -c:v libx264 -crf 20 -pix_fmt yuv420p -movflags +faststart "$out/mixer.mp4" || exit 1
ffmpeg -v error -y -framerate 12.8 -i "$out/frames/%04d.png"   -vf "scale=640:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=160:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle"   -loop 0 "$out/mixer.gif" || exit 1
ls -l "$out"/mixer.* "$out"/promo-*.png | awk '{print $5, $NF}'
echo "PROMO: done"
