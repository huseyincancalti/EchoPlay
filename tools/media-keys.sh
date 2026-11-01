#!/usr/bin/env bash
# Keyboards' media keys, headset buttons and Windows' media overlay: EchoPlay must be a media session of Windows, and a
# play/pause sent through that session must reach the player. A window on a silent clip for about 15 s; no real mouse or
# keyboard, no other app touched. It reads the real state.json and is killed, so it saves nothing.
# Usage: bash tools/media-keys.sh     Needs: bash tools/check.sh v2
# Exit: 0 = passed, 1 = a check failed, 3 = could not run
cd "$(dirname "$0")/.." || exit 3
. .out/devenv.sh || exit 3
[ -x .out/build-v2/EchoPlay.exe ] || { echo "No build: tools/check.sh v2"; exit 3; }
clip=.out/e2e/edge/silent-sound.mp4
mkdir -p .out/e2e/edge
[ -f "$clip" ] || ffmpeg -v error -f lavfi -i testsrc2=size=1280x720:rate=60 -f lavfi -i anullsrc=r=48000:cl=stereo -t 30 \
  -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a aac "$clip" || { echo "ffmpeg could not make the clip"; exit 3; }
ask() { powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File tools/media-keys.ps1 "$1" 2>&1 | tr -d '\r'; }
failed=0
status=
waitFor() { # waitFor <what> <status> : asked again until it says so, 10 s at most
  local until=$((SECONDS + 10))
  while :; do
    status=$(ask status)
    [ "${status%%|*}" = "$2" ] && { echo "  ok    $1"; return 0; }
    [ $SECONDS -lt $until ] || { echo "  FAIL  $1 (Windows says: ${status:-no session})"; failed=1; return 1; }
    sleep 0.5
  done
}

.out/build-v2/EchoPlay.exe "$(pwd)/$clip" &
app=$!
if waitFor "Windows has a media session of EchoPlay, playing" Playing; then
  [ "${status#*|}" = "$(basename "$clip")" ] && echo "  ok    the overlay shows its title" || { echo "  FAIL  the overlay's title: ${status#*|}"; failed=1; }
  ask toggle > /dev/null
  waitFor "play/pause from Windows pauses it" Paused && { ask toggle > /dev/null; waitFor "and plays it again" Playing; }
fi
kill "$app" 2>/dev/null
wait "$app" 2>/dev/null
[ $failed -eq 0 ] && echo "MEDIA-KEYS: passed" || echo "MEDIA-KEYS: FAILED"
exit $failed
