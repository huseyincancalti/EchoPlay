#!/usr/bin/env bash
# What only Windows itself can do with EchoPlay's frameless window, with the real mouse and keyboard: drag the resize
# edges and the caption, double-click the caption, hover the maximize button (snap layouts: look at the screenshot), Aero
# Snap by dragging to the top edge and with Win+Left. tools/e2e.sh covers the rest without real input.
# It TAKES THE MOUSE AND KEYBOARD for about 20 s: run it only when the maintainer says so. The pointer returns at the end.
# Usage: bash tools/window-live.sh        Needs: bash tools/check.sh v2 and bash tools/e2e.sh once (the clip).
# Output: .out/e2e/window-live.txt, screenshots in .out/shots (frame-snap-layouts, frame-snapped-left)
# Exit: 0 = passed, 1 = a check failed, 3 = could not run
cd "$(dirname "$0")/.." || exit 3
. .out/devenv.sh || exit 3
clip=.out/e2e/two-tracks-60s.mp4
[ -x .out/build-v2/tst_e2e.exe ] && [ -f "$clip" ] || { echo "run bash tools/check.sh v2 and bash tools/e2e.sh first"; exit 3; }
mkdir -p .out/shots
ECHOPLAY_E2E_REAL_INPUT=1 ECHOPLAY_E2E_SHOTS="$(pwd)/.out/shots" ECHOPLAY_E2E_CLIP="$(pwd)/$clip" ECHOPLAY_E2E_DIR="$(pwd)/.out/e2e/run" \
  .out/build-v2/tst_e2e.exe realWindowInput -o .out/e2e/window-live.txt,txt 2>/dev/null
code=$?
grep -E "^(PASS|FAIL!|SKIP|QINFO)|^ +(Actual|Expected|Loc)|^Totals" .out/e2e/window-live.txt
[ $code -eq 0 ] && { echo "WINDOW-LIVE: passed"; exit 0; }
echo "WINDOW-LIVE: FAILED. Full result: .out/e2e/window-live.txt"
exit 1
