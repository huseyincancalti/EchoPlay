#!/usr/bin/env bash
# Latency bench of the v2 seek-bar preview pictures (Thumbnailer): per video, how long the hidden libmpv player
# takes to open the file and show the first picture (cold), to serve a keyframe picture and the exact one at 8
# positions, and how much memory it adds. Headless: no window is opened, so it runs any time.
# Usage: bash tools/thumbs-bench.sh <video>...   Needs: bash tools/check.sh v2 (builds bench_thumbnailer).
# Output: one THUMBS line per video, also appended to .out/thumbs-bench.txt (see app/tests/bench_thumbnailer.cpp);
# the full output of a failed run is printed.
# Exit: 0 = measured, 1 = a run failed or timed out, 3 = could not run
cd "$(dirname "$0")/.." || exit 3
. .out/devenv.sh || exit 3
[ $# -gt 0 ] || { echo "usage: bash tools/thumbs-bench.sh <video>..."; exit 3; }
[ -x .out/build-v2/bench_thumbnailer.exe ] || { echo "bench_thumbnailer not built: run bash tools/check.sh v2"; exit 3; }
status=0
for video in "$@"; do
  [ -f "$video" ] || { echo "no such file: $video"; status=1; continue; }
  duration=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$video" | tr -d '\r')
  [ -n "$duration" ] || { echo "ffprobe could not read the duration of $video"; status=1; continue; }
  out=$(.out/build-v2/bench_thumbnailer.exe "$video" "$duration")
  code=$?
  echo "$out" | grep '^THUMBS ' | tee -a .out/thumbs-bench.txt
  if [ $code -ne 0 ] || echo "$out" | grep -q TIMEOUT; then
    echo "FAILED (exit $code): $video"
    echo "$out"
    status=1
  fi
done
exit $status
