#!/usr/bin/env bash
# Build/test evidence for EchoPlay. The output of this script is the evidence; a report is only a claim.
# Usage: bash tools/check.sh [v1|v2|all]     (default: all)
#   v1   Lua unit tests, luacheck, luac -p on mpv/scripts/echoplay-audio/
#   v2   app/: CMake configure + build + ctest (skipped until app/CMakeLists.txt exists)
# Prints one line per step; full log: .out/check-<mode>.log
# Exit: 0 = all passed, 1 = a step failed, 3 = nothing could run

mode="${1:-all}"
cd "$(dirname "$0")/.." || { echo "repo root not found"; exit 3; }
mkdir -p .out
LOG=".out/check-${mode}.log"
: > "$LOG"

ran=0; failed=0; skipped=()

step() { # $1 = label, rest = command. Output goes to the log; one status line on screen.
  local label="$1"; shift
  printf '\n===== %s: %s\n' "$label" "$*" >> "$LOG"
  if "$@" >> "$LOG" 2>&1; then
    echo "  ok    $label"
  else
    echo "  FAIL  $label (exit $?)"
    failed=$((failed + 1))
  fi
  ran=$((ran + 1))
}

have() { command -v "$1" >/dev/null 2>&1 || { skipped+=("$2: $1 not found"); return 1; }; }

check_v1() {
  echo "v1"
  local src=mpv/scripts/echoplay-audio
  have lua v1 && step "unit tests" lua tests/run.lua
  have luacheck v1 && step "luacheck" luacheck "$src"/*.lua
  have luac v1 && step "luac -p" luac -p "$src"/*.lua
}

check_v2() {
  if [ ! -f app/CMakeLists.txt ]; then skipped+=("v2: app/CMakeLists.txt does not exist yet"); return; fi
  [ -f .out/devenv.sh ] && . .out/devenv.sh  # written by tools/setup-dev.sh
  have cmake v2 || return
  echo "v2"
  step "cmake configure" cmake -S app -B .out/build-v2 -DCMAKE_BUILD_TYPE=Release
  step "cmake build" cmake --build .out/build-v2 --config Release
  # -LE e2e: the end-to-end test opens a window (tools/e2e.sh runs it).
  step "ctest" ctest --test-dir .out/build-v2 -C Release --output-on-failure -LE e2e
}

case "$mode" in
  v1)  check_v1 ;;
  v2)  check_v2 ;;
  all) check_v1; check_v2 ;;
  *)   echo "Usage: bash tools/check.sh [v1|v2|all]"; exit 3 ;;
esac

for s in "${skipped[@]}"; do echo "  skip  $s"; done

if [ "$ran" -eq 0 ]; then
  echo "CHECK: nothing ran. Log: $LOG"
  exit 3
fi
if [ "$failed" -gt 0 ]; then
  echo "CHECK: $failed/$ran steps failed. First error lines:"
  grep -E -i 'error|fail|warning' "$LOG" | head -n 12
  echo "Log: $LOG"
  exit 1
fi
echo "CHECK: $ran/$ran steps passed. Log: $LOG"
exit 0
