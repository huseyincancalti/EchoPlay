#!/usr/bin/env bash
# EchoPlay advanced/portable installer for Linux and macOS.
#   - Detects an existing mpv installation via `command -v mpv`.
#   - If missing, points the user at their platform's package manager (there's
#     no portable-binary download source for Linux/macOS the way
#     zhongfly/mpv-winbuild provides for Windows in install.ps1).
#   - Copies EchoPlay's config into mpv's config directory.
#   - Downloads uosc / thumbfast / memo, same sources as the release CI.
#   - Prints the manual "Open With" file-association step - there's no
#     reliable, scriptable, cross-distro/cross-macOS-version equivalent to
#     Windows's HKCU registry trick without extra tooling.
set -euo pipefail

UOSC_VERSION="5.12.0"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$ROOT/mpv"

CONFIG_DIR="${MPV_HOME:-${XDG_CONFIG_HOME:-$HOME/.config}/mpv}"

info() { printf '\033[36m[EchoPlay]\033[0m %s\n' "$1"; }
warn() { printf '\033[33m[EchoPlay]\033[0m %s\n' "$1"; }

usage() {
    cat <<EOF
Usage: $0 [--config-dir <path>]

  --config-dir <path>   Install EchoPlay's config into <path> instead of the
                         default mpv config directory (\$XDG_CONFIG_HOME/mpv
                         or ~/.config/mpv on both Linux and macOS).
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --config-dir)
            CONFIG_DIR="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if ! command -v mpv >/dev/null 2>&1; then
    warn "mpv not found on PATH."
    case "$(uname -s)" in
        Darwin)
            warn "Install it with Homebrew:  brew install mpv"
            ;;
        Linux)
            if command -v apt-get >/dev/null 2>&1; then
                warn "Install it with:  sudo apt install mpv"
            elif command -v dnf >/dev/null 2>&1; then
                warn "Install it with:  sudo dnf install mpv"
            elif command -v pacman >/dev/null 2>&1; then
                warn "Install it with:  sudo pacman -S mpv"
            else
                warn "Install mpv via your distro's package manager, then re-run this script."
            fi
            ;;
    esac
    warn "Continuing to install EchoPlay's config anyway - mpv just needs to exist before you can use it."
fi

info "Config directory: $CONFIG_DIR"
mkdir -p "$CONFIG_DIR/scripts" "$CONFIG_DIR/script-opts" "$CONFIG_DIR/fonts"

cp -R "$SRC"/. "$CONFIG_DIR"/
info "EchoPlay config copied."

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

info "Downloading uosc $UOSC_VERSION..."
curl -sfL -o "$TMP/uosc.zip" "https://github.com/tomasklaen/uosc/releases/download/$UOSC_VERSION/uosc.zip"
unzip -q "$TMP/uosc.zip" -d "$TMP/uosc"
cp -R "$TMP/uosc/scripts/." "$CONFIG_DIR/scripts/"
cp -R "$TMP/uosc/fonts/." "$CONFIG_DIR/fonts/"
# EchoPlay's own uosc.conf (mpv/script-opts/uosc.conf) was already copied above;
# this only fills it in if somehow missing.
[ -f "$CONFIG_DIR/script-opts/uosc.conf" ] || \
    curl -sfL -o "$CONFIG_DIR/script-opts/uosc.conf" "https://github.com/tomasklaen/uosc/releases/download/$UOSC_VERSION/uosc.conf"

info "Downloading thumbfast..."
curl -sfL -o "$CONFIG_DIR/scripts/thumbfast.lua" "https://raw.githubusercontent.com/po5/thumbfast/master/thumbfast.lua"

info "Downloading memo..."
curl -sfL -o "$CONFIG_DIR/scripts/memo.lua" "https://raw.githubusercontent.com/po5/memo/master/memo.lua"
[ -f "$CONFIG_DIR/script-opts/memo.conf" ] || \
    curl -sfL -o "$CONFIG_DIR/script-opts/memo.conf" "https://raw.githubusercontent.com/po5/memo/master/memo.conf"

cp "$ROOT/THIRD_PARTY_NOTICES.md" "$CONFIG_DIR/" 2>/dev/null || true

info "Done. mpv will now load EchoPlay's config from: $CONFIG_DIR"
echo
warn "To make mpv your default player for video/audio files:"
case "$(uname -s)" in
    Darwin)
        warn "  Finder -> right-click a video -> Get Info -> Open with -> mpv -> Change All..."
        ;;
    Linux)
        warn "  Your file manager -> right-click a video -> Open With -> Other Application -> browse to mpv -> set as default."
        ;;
esac
