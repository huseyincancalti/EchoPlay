#!/usr/bin/env bash
# What a never-seen EchoPlay costs the first time it starts (competitor report idea 3: Windows checks a new program on
# its first run). Copies the staged app (tools/package-v2.sh) to fresh folders and appends one byte to every .exe and
# .dll, so Windows sees new files, as after an install or an update. Then times, by the wall clock:
#   a) --smoke (libmpv starts, no window) three times: cold, then warm;
#   b) a 2 s --bench on a clip twice: cold, then warm (a window for a few seconds, no input);
#   c) --smoke once, then the same --bench: what a warm-up run at install would leave for the user's first start.
# Usage: bash tools/first-run.sh [clip]     Report: .out/first-run/report.txt
set -euo pipefail
cd "$(dirname "$0")/.."
STAGE=installer/windows/v2/stage
[ -x "$STAGE/EchoPlay.exe" ] || { echo "No staged app: run tools/package-v2.sh first"; exit 3; }
CLIP=$(cygpath -w "$PWD/${1:-.out/e2e/two-tracks-60s.mp4}")
OUT=.out/first-run
rm -rf "$OUT"
mkdir -p "$OUT"

now() { date +%s%3N; }
report() { echo "$*" | tee -a "$OUT/report.txt"; }
fresh() { # fresh <name>: the staged app, every exe and dll with one more byte (a PE file runs the same)
  cp -r "$STAGE" "$OUT/$1"
  find "$OUT/$1" \( -iname '*.exe' -o -iname '*.dll' \) -print0 | while IFS= read -r -d '' f; do printf 'x' >> "$f"; done
}
smoke() { # smoke <name>: ms
  local s; s=$(now)
  "$OUT/$1/EchoPlay.exe" --smoke > /dev/null 2>&1 || true
  echo $(($(now) - s))
}
bench() { # bench <name> <run>: "<wall ms> (first frame at <process_ms> of the process)"
  local s log="$OUT/$1-$2.log"
  s=$(now)
  timeout 60 "$OUT/$1/EchoPlay.exe" --bench 2 --log-file "$(cygpath -w "$PWD/$log")" "$CLIP" > /dev/null 2>&1 || true
  echo "$(($(now) - s)) ms (first frame at $(grep -o '"process_ms":[0-9]*' "$log" | head -1 | cut -d: -f2) ms of the process)"
}

report "a) --smoke on new files"
fresh a
report "  cold  $(smoke a) ms"
report "  warm  $(smoke a) ms"
report "  warm  $(smoke a) ms"
report "b) 2 s --bench on new files"
fresh b
report "  cold  $(bench b 1)"
report "  warm  $(bench b 2)"
report "c) --smoke first, then the 2 s --bench"
fresh c
report "  smoke $(smoke c) ms"
report "  bench $(bench c 1)"
