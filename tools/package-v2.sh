#!/usr/bin/env bash
# Builds the v2 Windows installer: a Release build, `cmake --install` into a clean stage, checks on the staged
# app, then Inno Setup. Output: installer/windows/v2/Output/EchoPlay-Setup.exe
# Checks: the staged EchoPlay.exe starts with Windows' own PATH only (no Qt, no MinGW, no libmpv elsewhere),
# then plays a generated clip and saves a screenshot of its window (--shot-at; a window shows for ~2 s).
# The setup itself is NOT run here: it would replace the EchoPlay (v1) installed on this machine (same AppId).
# Usage: [VERSION=2.0.0] bash tools/package-v2.sh    (default 0.0.0-dev: a dev build never checks for updates)
# Needs: tools/setup-dev.sh done, Inno Setup 6 (ISCC; default: per-user install). Log: .out/package-v2.log
set -euo pipefail
cd "$(dirname "$0")/.."
. .out/devenv.sh
VERSION="${VERSION:-0.0.0-dev}"
BUILD=.out/build-v2-package
STAGE=installer/windows/v2/stage
LOG=.out/package-v2.log
ISCC="${ISCC:-$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe}"
[ -x "$ISCC" ] || { echo "Inno Setup not found: $ISCC (set ISCC=...)"; exit 3; }

echo "build ($VERSION)"
cmake -S app -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DECHOPLAY_VERSION="$VERSION" > "$LOG" 2>&1
cmake --build "$BUILD" --target echoplay >> "$LOG" 2>&1
rm -rf "$STAGE"
cmake --install "$BUILD" --prefix "$PWD/$STAGE" >> "$LOG" 2>&1 # Qt's deploy step needs an absolute path

echo "check the staged app without the dev PATH"
clean() { env -i PATH="/c/Windows/System32:/c/Windows" SYSTEMROOT='C:\Windows' SYSTEMDRIVE='C:' \
  USERNAME="$USERNAME" LOCALAPPDATA="$LOCALAPPDATA" APPDATA="$APPDATA" USERPROFILE="$USERPROFILE" "$@"; }
clean "$STAGE/EchoPlay.exe" --smoke
mkdir -p .out/package
clip=.out/package/clip.mp4
[ -f "$clip" ] || ffmpeg -v error -f lavfi -i testsrc2=size=1280x720:rate=30 -t 3 -c:v libx264 -preset ultrafast \
  -pix_fmt yuv420p "$clip"
shot=.out/package/shot.png
rm -f "$shot"
clean "$STAGE/EchoPlay.exe" --shot-at 1 --shot-out "$(cygpath -m "$PWD/$shot")" "$(cygpath -m "$PWD/$clip")" || true
size=$(stat -c %s "$shot" 2>/dev/null || echo 0)
[ "$size" -gt 20000 ] || { echo "FAIL: the staged app did not render the clip ($shot: $size bytes)"; exit 1; }
echo "  ok    staged app played the clip ($shot, $size bytes)"

echo "installer"
# MSYS_NO_PATHCONV: Git Bash would turn the /Q and /D switches into paths.
MSYS_NO_PATHCONV=1 "$ISCC" /Q /DMyAppVersion="$VERSION" /DMyFileVersion="${VERSION%%-*}" installer/windows/v2/EchoPlay.iss >> "$LOG" 2>&1
setup=installer/windows/v2/Output/EchoPlay-Setup.exe
echo "PACKAGE: $setup ($(du -h "$setup" | cut -f1), staged app $(du -sh "$STAGE" | cut -f1)). Log: $LOG"
