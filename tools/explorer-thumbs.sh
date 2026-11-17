#!/usr/bin/env bash
# Explorer thumbnails of the common video types: does Windows draw a frame of the clip, or only the icon of the app the
# type opens with (EchoPlay's logo)? Fresh clips in a new folder each run, so Windows' thumbnail cache cannot answer.
# Headless: no window, no input. It prints which thumbnail handler each type has, then the result per file.
# Usage: bash tools/explorer-thumbs.sh
# Exit: 0 = every type has a thumbnail, 1 = some show only the icon, 3 = could not run
cd "$(dirname "$0")/.." || exit 3
. .out/devenv.sh 2>/dev/null
command -v ffmpeg > /dev/null || { echo "No ffmpeg on PATH"; exit 3; }
dir=.out/explorer-thumbs/$(date +%Y%m%d-%H%M%S)
mkdir -p "$dir" || exit 3
src=(-v error -f lavfi -i testsrc2=size=1280x720:rate=30 -f lavfi -i sine=frequency=440:sample_rate=48000 -t 2)
make() { ffmpeg "${src[@]}" "$@" || { echo "ffmpeg could not make $*"; exit 3; }; }
make -c:v libx264 -pix_fmt yuv420p -c:a aac "$dir/h264.mp4"
# A ShadowPlay / NVIDIA App clip: H.264 and two AAC tracks (game, mic).
ffmpeg -v error -f lavfi -i testsrc2=size=1280x720:rate=30 -f lavfi -i sine=frequency=440:sample_rate=48000 \
  -f lavfi -i sine=frequency=880:sample_rate=48000 -t 2 -map 0 -map 1 -map 2 -c:v libx264 -pix_fmt yuv420p -c:a aac \
  "$dir/two-tracks.mp4" || exit 3
make -c:v libx265 -tag:v hvc1 -pix_fmt yuv420p -c:a aac "$dir/hevc.mp4"
make -c:v libx264 -pix_fmt yuv420p -c:a aac "$dir/h264.mkv"
make -c:v libx264 -pix_fmt yuv420p -c:a aac "$dir/h264.mov"
make -c:v libx264 -pix_fmt yuv420p -c:a aac "$dir/h264.m4v"
make -c:v libvpx-vp9 -deadline realtime -c:a libopus "$dir/vp9.webm"
make -c:v mpeg4 -c:a mp3 "$dir/mpeg4.avi"
make -c:v libx264 -pix_fmt yuv420p -c:a aac "$dir/h264.ts"

echo "Thumbnail handler per type (HKCR\\.ext\\ShellEx, and on the ProgID the type opens with):"
powershell.exe -NoProfile -NonInteractive -Command '
$g = "{e357fccd-a995-4576-b01f-234630154e96}"
foreach ($e in ".mp4",".mkv",".mov",".m4v",".webm",".avi",".ts") {
  $choice = (Get-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\$e\UserChoice" -ErrorAction SilentlyContinue).ProgId
  $onExt = (Get-ItemProperty "Registry::HKEY_CLASSES_ROOT\$e\ShellEx\$g" -ErrorAction SilentlyContinue)."(default)"
  $onProg = if ($choice) { (Get-ItemProperty "Registry::HKEY_CLASSES_ROOT\$choice\ShellEx\$g" -ErrorAction SilentlyContinue)."(default)" }
  $name = { param($id) if ($id) { (Get-ItemProperty "Registry::HKEY_CLASSES_ROOT\CLSID\$id" -ErrorAction SilentlyContinue)."(default)" } else { "-" } }
  # Windows own extractor (shell32) reads the clip through the type property handler: one replaced by another
  # program (Icaros does that) makes it fail.
  $prop = (Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\$e" -ErrorAction SilentlyContinue)."(default)"
  "  {0,-5} opens with {1,-28} thumbnails: {2} (on its ProgID: {3}); properties: {4}" -f $e, $choice, (& $name $onExt), (& $name $onProg), (& $name $prop)
}' | tr -d '\r'

echo "Thumbnails (thumbnail only, no icon fallback):"
failed=0
while IFS='|' read -r name state ms hr; do
  if [ "$state" = ok ]; then echo "  ok    $name ($ms ms)"; else echo "  FAIL  $name: only the icon ($hr, $ms ms)"; failed=1; fi
done < <(powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File tools/explorer-thumbs.ps1 "$(cygpath -w "$dir")" | tr -d '\r')
[ $failed = 0 ] && echo "EXPLORER-THUMBS: passed" || echo "EXPLORER-THUMBS: some types show only the icon"
exit $failed
