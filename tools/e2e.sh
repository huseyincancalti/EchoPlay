#!/usr/bin/env bash
# End-to-end check of the v2 player: opens the real player window on a generated two-track clip, presses the
# keys a user would (mixer Ctrl+1/2/3, volume, pause, speed, seek, full screen, double-click), checks what mpv
# does and what is saved, then starts the player again and checks that the mixer and volume came back.
# Muted, about 20 s. A window appears (briefly full screen); it needs no quiet screen: the synthetic input
# goes to the test window only, not to the OS.
# Usage: bash tools/e2e.sh          Needs: bash tools/check.sh v2 (builds tst_e2e).
# Output: .out/e2e/result.txt; state files and mpv logs of the last run in .out/e2e/run/
# Exit: 0 = passed, 1 = a check failed, 3 = could not run
cd "$(dirname "$0")/.." || exit 3
. .out/devenv.sh || exit 3
[ -x .out/build-v2/tst_e2e.exe ] || { echo "tst_e2e not built: run bash tools/check.sh v2"; exit 3; }
mkdir -p .out/e2e
clip=.out/e2e/two-tracks-60s.mp4 # 60 s: resume keeps 15 s .. duration-20 s
if [ ! -f "$clip" ]; then
  ffmpeg -v error -f lavfi -i testsrc2=size=1280x720:rate=60 -f lavfi -i sine=frequency=440 \
    -f lavfi -i sine=frequency=880 -t 60 -map 0:v -map 1:a -map 2:a -c:v libx264 -preset ultrafast \
    -pix_fmt yuv420p -c:a aac -metadata:s:a:0 title="System sounds" -metadata:s:a:1 title="Microphone" "$clip" \
    || { echo "ffmpeg could not make the test clip"; exit 3; }
fi
# Qt Test prints nothing to a redirected stdout here (Qt 6.10 MinGW): results go to a file.
ECHOPLAY_E2E_CLIP="$(pwd)/$clip" ECHOPLAY_E2E_DIR="$(pwd)/.out/e2e/run" .out/build-v2/tst_e2e.exe -o .out/e2e/result.txt,txt
code=$?
grep -E "^(PASS|FAIL!|XFAIL|SKIP)|^ +(Actual|Expected|Loc)|^Totals" .out/e2e/result.txt
[ $code -eq 0 ] && echo "E2E: passed" || echo "E2E: FAILED (exit $code). Full result: .out/e2e/result.txt"
[ $code -eq 0 ] || exit 1
