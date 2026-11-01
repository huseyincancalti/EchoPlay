#!/usr/bin/env bash
# The real setup and the real app, in a test install: an update that installs when EchoPlay closes, a clip opened right
# after the close while the setup runs, the same without the guard, and Install now. The first install too.
# Judged by processes, files and mpv's log, so it needs no picture and works with the display asleep. When the display
# is on, windows show for a few seconds; no real mouse or keyboard.
# The test install has its own AppId and folder, no uninstall entry, no shortcuts and no registry (TestDir in
# installer/windows/v2/EchoPlay.iss): the EchoPlay on this machine is compared before and after. It reads the real
# state.json, as tools/package-v2.sh's staged run does.
# Usage: bash tools/package-v2.sh && bash tools/setup-live.sh     Report: .out/setup-test/report.txt     Exit 1: a check failed
set -euo pipefail
cd "$(dirname "$0")/.."
. .out/devenv.sh
OUT=.out/setup-test
APP=$OUT/app
STAGE=installer/windows/v2/stage
ISCC="${ISCC:-$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe}"
[ -x "$ISCC" ] || { echo "Inno Setup not found: $ISCC (set ISCC=...)"; exit 3; }
[ -x "$STAGE/EchoPlay.exe" ] || { echo "No staged app: run tools/package-v2.sh first"; exit 3; }

clean() { env -i PATH="/c/Windows/System32:/c/Windows" SYSTEMROOT='C:\Windows' SYSTEMDRIVE='C:' \
  USERNAME="$USERNAME" LOCALAPPDATA="$LOCALAPPDATA" APPDATA="$APPDATA" USERPROFILE="$USERPROFILE" "$@"; }
now() { date +%s%3N; }
failures=0
report() { echo "$*" | tee -a "$OUT/report.txt"; }
check() { # check <what> <condition...>
  local what=$1; shift
  if "$@"; then report "  ok    $what"; else report "  FAIL  $what"; failures=$((failures + 1)); fi
}
setupRunning() { tasklist //FO CSV //NH //FI "IMAGENAME eq EchoPlay-Setup-test.exe" 2>/dev/null | grep -qi "EchoPlay-Setup-test"; }
waitForSetup() { # waits for the hidden setup to end; false when it never did
  local start; start=$(now)
  while setupRunning; do
    [ $(($(now) - start)) -lt 120000 ] || return 1
    sleep 0.1
  done
}
playedThatClip() { # playedThatClip <log> <ms>: the launch came back, with its path whole, and mpv opened it
  local start; start=$(now)
  until grep -qF 'clip & 100% (a).mp4' "$1" 2>/dev/null && grep -q 'VO: \[' "$1" 2>/dev/null; do
    [ $(($(now) - start)) -lt "$2" ] || return 1
    sleep 0.2
  done
}
testAppRunning() {
  [ "$(powershell.exe -NoProfile -Command "@(Get-Process EchoPlay -ErrorAction SilentlyContinue | Where-Object { \$_.Path -like '*setup-test*' }).Count" 2>/dev/null | tr -d '\r')" != "0" ]
}
notRunning() { ! testAppRunning; }
killTestApp() { # only processes from the test folder, never a real EchoPlay. Its files are free when this returns.
  powershell.exe -NoProfile -Command "Get-Process EchoPlay -ErrorAction SilentlyContinue | Where-Object { \$_.Path -like '*setup-test*' } | Stop-Process -Force" 2>/dev/null || true
  local start; start=$(now)
  while testAppRunning && [ $(($(now) - start)) -lt 5000 ]; do sleep 0.1; done
  sleep 0.3
}
sameAsStage() { # every staged file is installed, byte for byte
  local different
  different=$(diff -rq "$STAGE" "$APP" 2>/dev/null | grep -E "differ|Only in $STAGE" || true)
  [ -z "$different" ]
}
oldVersion() { # what the setup must put right, as an update does. The exe and a DLL too: a file a running EchoPlay
  # holds is what an update can fail on, and a byte at the end of a PE file changes nothing when it runs.
  rm -f "$APP/lang/tr.json"
  echo '{}' > "$APP/theme.json"
  printf 'x' >> "$APP/EchoPlay.exe"
  printf 'x' >> "$APP/libmpv-2.dll"
}
theRealEchoPlay() { # fingerprint of what is installed on this machine: must not move
  local real="$LOCALAPPDATA/Programs/EchoPlay"
  ( cd "$real" 2>/dev/null && find . -type f -printf '%p %s %T@\n' | sort | sha256sum )
  reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\{6E9F2C9C-6E61-4A8B-9A5B-2F1E0E6F4C2D}_is1' //v DisplayVersion 2>/dev/null || true
  reg query 'HKCU\Software\Classes\Applications\EchoPlay.exe\shell\open\command' 2>/dev/null || true
}
closeWithUpdate() { clean "$APP/EchoPlay.exe" --smoke --setup-on-close "$SETUPW" > /dev/null; } # the real close path, no window

killTestApp
rm -rf "$APP" "$OUT/logs"
mkdir -p "$OUT/logs"
: > "$OUT/report.txt"
before=$(theRealEchoPlay)

report "test setup (the real script, its real compression)"
# MSYS_NO_PATHCONV: Git Bash would turn the /Q and /D switches into paths.
MSYS_NO_PATHCONV=1 "$ISCC" /Q "/DTestDir=$(cygpath -w "$PWD/$APP")" "/O$(cygpath -w "$PWD/$OUT")" /FEchoPlay-Setup-test \
  installer/windows/v2/EchoPlay.iss > "$OUT/iscc.log" 2>&1
SETUP=$OUT/EchoPlay-Setup-test.exe
SETUPW=$(cygpath -w "$PWD/$SETUP")
report "  $(du -h "$SETUP" | cut -f1)"
# A & and a % in the name: the guard hands this path through cmd.exe.
CLIP="$OUT/clip & 100% (a).mp4"
[ -f "$CLIP" ] || ffmpeg -v error -f lavfi -i testsrc2=size=1280x720:rate=30 -t 12 -c:v libx264 -preset ultrafast -pix_fmt yuv420p "$CLIP"
CLIPM=$(cygpath -m "$PWD/$CLIP")

report "first install"
start=$(now)
code=0; clean "$SETUP" //VERYSILENT //SUPPRESSMSGBOXES //NORESTART || code=$?
report "  $(($(now) - start)) ms, exit $code"
check "it installed every staged file" sameAsStage

report "update on close, nothing else opened"
oldVersion
closed=$(now)
closeWithUpdate
check "the setup ended" waitForSetup
report "  the setup took about $(($(now) - closed)) ms"
check "it replaced what was old" sameAsStage
check "EchoPlay stayed closed" notRunning

report "update on close, a clip opened right after the close and two more during the setup"
oldVersion
closeWithUpdate
closed=$(now)
leaves=()
for n in 1 2 3; do # the setup is quick: only the first is sure to land inside it, a later one may just open normally
  start=$(now)
  clean "$APP/EchoPlay.exe" --bench 1 --log-file "$(cygpath -m "$PWD/$OUT/logs/during-$n.log")" "$CLIPM" &
  launch=$!
  while kill -0 "$launch" 2>/dev/null && [ $(($(now) - start)) -lt 2000 ]; do sleep 0.02; done
  if kill -0 "$launch" 2>/dev/null; then leaves+=(stayed); else leaves+=($(($(now) - start))); fi
  [ "$n" = 3 ] || sleep 0.1
done
check "the setup ended" waitForSetup
report "  the setup took about $(($(now) - closed)) ms; the three launches left after: ${leaves[*]} (ms; stayed = it opened normally)"
check "the first launch left at once and held no file" [ "${leaves[0]}" != stayed ]
check "the update replaced what was old" sameAsStage
for n in 1 2 3; do
  check "clip $n came back when the setup was done, its path whole" playedThatClip "$OUT/logs/during-$n.log" 30000
done
killTestApp

report "the same without the guard (--after-setup): what the guard prevents. Not a check"
oldVersion
closeWithUpdate
sleep 0.2
L="$OUT/logs/unguarded.log"
clean "$APP/EchoPlay.exe" --after-setup --bench 30 --log-file "$(cygpath -m "$PWD/$L")" "$CLIPM" &
unguarded=$!
waitForSetup || true
if kill -0 "$unguarded" 2>/dev/null; then report "  its window: still open when the setup ended"; else report "  its window: CLOSED by the setup"; fi
report "  the files: $(sameAsStage && echo 'all replaced' || echo 'NOT all replaced')"
killTestApp
wait "$unguarded" 2>/dev/null || true
clean "$SETUP" //VERYSILENT //SUPPRESSMSGBOXES //NORESTART || true # back to a whole install

report "Install now reopens EchoPlay"
oldVersion
start=$(now)
code=0; clean "$SETUP" //VERYSILENT //SUPPRESSMSGBOXES //NORESTART //ECHOPLAYUPDATE=1 || code=$?
report "  $(($(now) - start)) ms, exit $code"
check "the update replaced what was old" sameAsStage
sleep 1.5
check "the new EchoPlay is up" testAppRunning
killTestApp

after=$(theRealEchoPlay)
check "the EchoPlay installed on this machine did not change" [ "$before" = "$after" ]

rm -rf "$APP"
[ "$failures" = 0 ] && report "SETUP-LIVE: passed" || { report "SETUP-LIVE: $failures failed"; exit 1; }
