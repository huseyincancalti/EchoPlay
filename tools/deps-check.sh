#!/usr/bin/env bash
# Would the staged app (tools/package-v2.sh) start on a Windows with nothing else installed? Lists every DLL that the
# staged .exe and .dll files import and checks that each is either in the stage itself or a file of Windows' own: in
# System32 and signed as part of the operating system. A Visual C++ or DirectX runtime that some other program put
# into System32 is not, and on a clean machine it would be missing.
# The yardstick is this PC's Windows: it does not tell what an older Windows lacks. Nothing is started (the real test
# is the setup on a clean machine). Headless, a few seconds.
# Usage: bash tools/deps-check.sh       Report: .out/deps-check.txt
# Exit: 0 = every import is covered, 1 = something would be missing, 3 = could not run
cd "$(dirname "$0")/.." || exit 3
. .out/devenv.sh || exit 3
STAGE=installer/windows/v2/stage
[ -x "$STAGE/EchoPlay.exe" ] || { echo "No staged app: run tools/package-v2.sh first"; exit 3; }
command -v objdump > /dev/null || { echo "objdump not found (tools/setup-dev.sh installs MinGW)"; exit 3; }
out=.out/deps-check.txt

# "<dll>\t<importer>", lower case, one line per import.
find "$STAGE" \( -iname '*.exe' -o -iname '*.dll' \) -print0 | while IFS= read -r -d '' f; do
  objdump -p "$f" 2> /dev/null | tr -d '\r' | sed -n 's/^[[:space:]]*DLL Name: //p' | tr 'A-Z' 'a-z' | sed "s|\$|\t${f#"$STAGE"/}|"
done | sort -u > .out/deps-imports.txt
[ -s .out/deps-imports.txt ] || { echo "objdump listed no imports"; exit 3; }
find "$STAGE" \( -iname '*.exe' -o -iname '*.dll' \) -printf '%f\n' | tr 'A-Z' 'a-z' | sort -u > .out/deps-own.txt
# api-ms-win-* and ext-ms-* are names Windows maps to its own DLLs, not files.
cut -f1 .out/deps-imports.txt | sort -u | grep -v -e '^api-ms-win-' -e '^ext-ms-' | grep -vxFf .out/deps-own.txt > .out/deps-outside.txt

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/deps-check.ps1 "$(cygpath -w "$PWD/.out/deps-outside.txt")" | tr -d '\r' > "$out"
bad=$(grep -cv $'\twindows$' "$out")
if [ "$bad" -eq 0 ]; then
  echo "DEPS: passed. $(wc -l < .out/deps-imports.txt) imports; $(wc -l < "$out") DLLs outside the stage, all Windows' own. Report: $out"
  exit 0
fi
grep -v $'\twindows$' "$out" | while IFS=$'\t' read -r dll verdict; do
  echo "FAIL: $dll ($verdict), imported by: $(grep -F "$dll"$'\t' .out/deps-imports.txt | cut -f2 | tr '\n' ' ')"
done
echo "DEPS: FAILED. $bad DLLs would be missing on a clean Windows. Report: $out"
exit 1
