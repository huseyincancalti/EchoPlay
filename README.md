<div align="center">

<img src="assets/logo.png" alt="EchoPlay — video player for NVIDIA ShadowPlay and OBS Studio dual audio track recordings on Windows, macOS, and Linux" width="160" />

# EchoPlay

**The video player for dual-audio-track recordings — NVIDIA ShadowPlay, NVIDIA App, and OBS
Studio clips with separate game/mic tracks, mixed live. Windows, macOS, and Linux.**

[![Latest release](https://img.shields.io/github/v/release/huseyincancalti/EchoPlay?label=download&color=ff7d26)](https://github.com/huseyincancalti/EchoPlay/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-0078D6)](#download--install)

</div>

---

## What is EchoPlay? A player built for NVIDIA ShadowPlay & OBS dual audio tracks

EchoPlay is a curated **distribution of [mpv](https://mpv.io)** — *not a fork*. It wraps the
upstream engine with a modern [uosc](https://github.com/tomasklaen/uosc) interface and a single
small script, so you get a clean, YouTube-style player that adds the features mpv leaves to you.

It was born from one annoyance: **NVIDIA ShadowPlay**, the **NVIDIA App** overlay, and **OBS
Studio** (recording with "Track 1 = desktop/game audio, Track 2 = microphone") all save clips with
**two separate audio tracks** instead of one mixed track. Most players force you to pick a single
track, or make you re-encode the file in an editor just to mix them. EchoPlay mixes any combination
of tracks **live, with no re-encode and no reloading the video** — and remembers your choice for
next time.

### Who EchoPlay is for

- 🎮 **NVIDIA ShadowPlay / NVIDIA App users** whose Instant Replay or manual recordings capture
  game and mic audio on separate tracks.
- 🎥 **OBS Studio streamers & recorders** using multi-track audio output (desktop + mic on
  separate tracks) who want to remix a clip after recording without re-exporting from OBS.
- ✂️ **Editors and YouTubers** who receive dual-track clips from someone else and just need to
  balance, mute, or re-center a track before uploading.
- 🎧 Anyone with a **quiet or channel-imbalanced recording** who wants louder, centered audio
  without opening Audacity or Premiere.

### Highlights

- 🎚️ **Live audio mixer** — toggle each track on/off, set per-track volume, and merge them in real
  time. A per-track **Mono** switch re-centers a channel-imbalanced track (e.g. a mic that only
  comes through the left speaker).
- 🔊 **Built-in loudness boost** — quiet recordings come out strong (`dynaudnorm`); turning the
  volume up is already a powerful boost.
- 🪟 **Modern UI** — YouTube-style timeline, hover thumbnails ([thumbfast](https://github.com/po5/thumbfast)),
  watch history & resume ([memo](https://github.com/po5/memo)), a right-edge volume slider, and clean
  keyboard feedback (your own volume / seek indicators, never the system's white bar).
- ⚙️ **One searchable Settings menu** — right-click: audio sources, video speed, picture quality,
  theme, language, and file/subtitle/playlist shortcuts, grouped into clear, color-coded sections.
- 🎨 **Theming** — clean Dark / Light modes plus an accent color, switched live. Drop-in community
  theme & language packs (see below).
- ⌨️ **Every shortcut is yours to remap** — click any shortcut in Settings and press the key or
  combo you want (up to 3 keys at once); only right-click into the menu itself stays fixed.
- 🖥️ **Three real picture-quality tiers** — Ultra (zero compromise), Normal (balanced default),
  Low (weak hardware), or Auto to let EchoPlay pick and adapt live as playback runs.
- 💾 **Persistent, and it asks before resuming** — your mixer, mono, accent, language, speed, and
  shortcut choices survive restarts. Playback always starts at 0:00 and offers to jump back to
  where you left off — it never silently skips ahead.

### EchoPlay vs. VLC, PotPlayer & vanilla mpv for dual-audio-track video

| | VLC / PotPlayer | Vanilla mpv | **EchoPlay** |
|---|---|---|---|
| Play a specific audio track | ✅ (pick one) | ✅ (pick one) | ✅ |
| **Mix multiple tracks together, live** | ❌ | ⚙️ manual `lavfi-complex` | ✅ one click |
| Per-track volume / mute | ❌ | ⚙️ manual | ✅ |
| Fix a mic that's only in one ear (mono) | ❌ | ⚙️ manual | ✅ one click |
| Remembers your mix per file | ❌ | ❌ | ✅ |
| Setup | Install only | Install + hand-written config | **One installer, ready to go** |

## Download & Install

**No prerequisites, no terminal.** Everything (player engine, UI, fonts) is bundled — pick your
platform below. Windows, macOS, and Linux downloads are all on the
[latest release](https://github.com/huseyincancalti/EchoPlay/releases/latest) page.

<details open>
<summary><strong>Windows</strong></summary>

1. Download **EchoPlay-Setup.exe** from the latest release.
2. Double-click it and click through the wizard (no admin rights needed — it installs to your
   user folder).
3. To make it your default player: right-click any video → **Open with → EchoPlay → Always**.

That's it — open a video, right-click for **Settings**. "EchoPlay" is what Windows
shows everywhere: the taskbar, Task Manager, and the "Open with" list — never "mpv".

<details>
<summary>Advanced: portable / scripted install (<code>install.ps1</code>)</summary>

If you already manage your own mpv (e.g. via scoop) or want a portable, no-installer layout,
download the source zip ("Code → Download ZIP") and run:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1
```

The script: detects mpv (or downloads a portable build automatically if you don't have one),
copies the EchoPlay config in, downloads uosc / thumbfast / memo, and registers the file
association. Flags: `-ConfigDir "C:\path"` to choose the config dir · `-Portable` to force a
portable layout · `-SkipMpvDownload` to never auto-download mpv · `-SkipAssoc` to skip the file
association.

</details>
</details>

<details>
<summary><strong>macOS</strong></summary>

1. Download **EchoPlay-\<version\>-arm64.dmg** (Apple Silicon: M1/M2/M3/M4) or
   **EchoPlay-\<version\>-x86_64.dmg** (Intel Mac) from the latest release.
2. Open the DMG and drag **EchoPlay** into **Applications**.
3. **First launch only:** macOS will say the app "is damaged" or that it "cannot verify the
   developer" — this is expected, EchoPlay isn't notarized by Apple (that requires a paid Apple
   Developer account). Right-click (or Control-click) **EchoPlay** in Applications, choose
   **Open**, then confirm **Open** in the dialog that appears. You only need to do this once.
4. To make it your default player: right-click a video in Finder → **Get Info → Open with →
   EchoPlay → Change All...**

<details>
<summary>Advanced: portable / scripted install (<code>install.sh</code>)</summary>

If you already have mpv (e.g. via `brew install mpv`) or just want EchoPlay's config without the
signed `.app`, download the source zip ("Code → Download ZIP") and run:

```bash
bash install.sh
```

The script copies EchoPlay's config into `~/.config/mpv`, downloads uosc / thumbfast / memo, and
prints the manual "Open with" steps (there's no scriptable file-association trick on macOS the way
Windows has one). Flag: `--config-dir <path>` to choose a different config dir.

</details>
</details>

<details>
<summary><strong>Linux</strong></summary>

1. Download **EchoPlay-\<version\>-x86_64.AppImage** from the latest release.
2. Make it executable: `chmod +x EchoPlay-*.AppImage`, then double-click it (or run it from a
   terminal). Some file managers need "Allow executing file as program" checked in the file's
   Properties first.
3. To make it your default player: right-click a video in your file manager → **Open With →
   Other Application** → browse to the AppImage → set it as default.

<details>
<summary>Advanced: portable / scripted install (<code>install.sh</code>)</summary>

If you already have mpv via your distro's package manager, download the source zip
("Code → Download ZIP") and run:

```bash
bash install.sh
```

The script copies EchoPlay's config into `~/.config/mpv`, downloads uosc / thumbfast / memo, and
prints the manual "Open With" steps. Flag: `--config-dir <path>` to choose a different config dir.

</details>
</details>

### Keyboard shortcuts

Every shortcut below is a default — click it in **Settings → Help → Keyboard Shortcuts** and
press whatever key or combo (1 to 3 keys) you'd rather use instead. Right-click is the one
exception, kept fixed as the guaranteed way into the menu.

| Key | Action |
|-----|--------|
| Right-click | Settings menu (not remappable) |
| `↑` / `↓` | Volume up / down (hold to ramp) |
| `←` / `→` | Seek 10s back / forward |
| `m` | Mute |
| `Space` | Pause / play |
| `g` | Toggle fixed speed (factor set in the menu) |
| `s` / `d` | Slow down / speed up (step is customizable in the menu) |
| `Ctrl`+`1/2/3` | Toggle audio track 1 / 2 / 3 |
| `F1` | Screenshot (with subtitles) |
| `Shift`+`s` | Screenshot (video only) |
| `Ctrl`+`s` | Screenshot (full window) |

Screenshot save location (including a "choose folder" picker) is also available from the
Settings menu.

### FAQ

**Why does my NVIDIA ShadowPlay / OBS recording have two audio tracks?**
NVIDIA's recorder and OBS Studio (with multi-track audio enabled) both save game/system sound and
your microphone as two independent audio tracks in the same video file, instead of mixing them
into one. Most players only let you pick a single track to play.

**How do I play a video with 2 audio tracks on Windows, macOS, or Linux?**
Set EchoPlay as your default player (see [Download & Install](#download--install) above), then
open the file — every audio track is auto-detected and shown in the mixer, and EchoPlay mixes
them for you by default.

**Can I merge two audio tracks without re-encoding the video?**
Yes — that's the whole point. EchoPlay rebuilds mpv's audio filter graph live (`lavfi-complex`),
so the video stream is never touched and there's no export or re-encode step.

**Does EchoPlay work with OBS Studio recordings?**
Yes. Any file with multiple audio tracks — OBS multi-track output, ShadowPlay, NVIDIA App, or
manually muxed files — is handled the same way.

---

## For the curious: how it works

EchoPlay is deliberately thin. The only original code is one Lua script
([`mpv/scripts/echoplay-audio/`](mpv/scripts/echoplay-audio/)); everything else is upstream
mpv, uosc, thumbfast and memo, wired together by config files. That makes it easy to extend — and
easy to keep close to upstream.

### Adding or editing a language

The entire UI is built from JSON string files, so translating EchoPlay needs **no code**:

1. In the player, open **Settings → Language → Open language folder** (this is `script-opts/`).
2. Copy `echoplay-en.json` and rename it `echoplay-<code>.json` (e.g. `echoplay-de.json`).
3. Translate the values and set `"lang_name"` to the language's own name (e.g. `"Deutsch"`).
4. Restart mpv — your language now appears in the **Language** menu automatically.

To make it official, open a pull request adding the file under `mpv/script-opts/`.

### Adding or editing a theme

Built-in **Dark / Light + accent color** cover the basics and live in **Settings → Theme**. For a
fuller custom look, a theme pack can override *any* uosc option (colors, opacity, corner radius…):

1. Open **Settings → Theme → Open theme folder** (this is `themes/`).
2. Copy [`amoled.conf`](mpv/themes/amoled.conf), rename it, and edit the values. The first
   `# Comment` line is the name shown in the menu; every other line is a uosc option, e.g.
   `color=foreground=ff5500`, `border_radius=14`, `opacity=menu=0.85`. Full option list:
   <https://github.com/tomasklaen/uosc>.
3. Save — it shows up under **Custom Themes** and applies live.

Share it with a pull request under `mpv/themes/`. See [CONTRIBUTING.md](CONTRIBUTING.md) for more.

### The audio engine

The trick is mpv's **`lavfi-complex`** property. Setting it at runtime rebuilds only the *audio*
filter graph — the video path is never touched, so switching tracks is instant and seamless. From
the set of enabled tracks the script builds a filter and assigns it live:

```text
# two tracks enabled, track 2 boosted to 200% and forced mono:
[aid1]volume=1.00[ev1];
[aid2]volume=2.00,pan=stereo|c0=0.5*c0+0.5*c1|c1=0.5*c0+0.5*c1[ev2];
[ev1][ev2]amix=inputs=2:normalize=0[ao]
```

- **One track at default volume** → no filter at all (native `aid=N`), zero overhead.
- **One track, custom volume or mono** → a single `volume` / `pan` chain.
- **Two or more** → each track gets its own `volume`(`+pan`) stage, then `amix` blends them.

Loudness is handled separately by `af=dynaudnorm=...` in [`mpv.conf`](mpv/mpv.conf), and master
volume goes up to 300%. The settings menu, mixer, theming and persistence (a small
`echoplay-state.json`) are all driven from the one Lua script via uosc's menu API.

### Project layout

| Path | Purpose |
|------|---------|
| [`mpv/scripts/echoplay-audio/`](mpv/scripts/echoplay-audio/) | EchoPlay's only original code — `main.lua` (mixer, menus, theming, shortcuts) + `i18n.lua`/`quality.lua`/`resume.lua` |
| [`mpv/script-opts/echoplay-*.json`](mpv/script-opts) | Language packs |
| [`mpv/themes/*.conf`](mpv/themes) | Community theme packs |
| [`mpv/mpv.conf`](mpv/mpv.conf), [`mpv/input.conf`](mpv/input.conf) | Player settings & shortcuts (shared across all 3 platforms) |
| [`installer/windows/EchoPlay.iss`](installer/windows/EchoPlay.iss) | Inno Setup script that builds `EchoPlay-Setup.exe` |
| [`installer/macos/Info.plist.template`](installer/macos/Info.plist.template), [`entitlements.plist`](installer/macos/entitlements.plist) | macOS `.app` bundle identity (file associations, icon) + ad-hoc signing entitlements |
| [`installer/linux/echoplay.desktop`](installer/linux/echoplay.desktop), [`AppRun`](installer/linux/AppRun) | Linux AppImage desktop entry (name/icon/mime types) + launcher |
| [`.github/workflows/release.yml`](.github/workflows/release.yml) | CI: builds and publishes `EchoPlay-Setup.exe`, macOS `.dmg` (arm64 + x86_64), and the Linux `.AppImage` in one release |
| [`.github/workflows/ci.yml`](.github/workflows/ci.yml) | CI: lint (luacheck) + headless smoke test on every push/PR |
| [`install.ps1`](install.ps1) | Windows advanced/portable installer (no GUI wizard) |
| [`install.sh`](install.sh) | Linux/macOS advanced/portable installer |
| [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), [`licenses/`](licenses/) | Bundled upstream license notices (uosc, memo, thumbfast, mpv) |

## Credits & license

EchoPlay's own code (`mpv/scripts/echoplay-audio/` + config/JSON/theme files) is **MIT** — see
[LICENSE](LICENSE). It downloads and bundles, under their own licenses:
[uosc](https://github.com/tomasklaen/uosc) (LGPL-2.1) ·
[memo](https://github.com/po5/memo) (GPL-3.0) ·
[thumbfast](https://github.com/po5/thumbfast) (MPL-2.0) ·
and of course [mpv](https://mpv.io) itself (GPL-2.0/LGPL-2.1). Full license texts
are in [`licenses/`](licenses/) — see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
for details.

---

<div align="center">

Made by **Hüseyin Can ÇALTI**
[hsyncalti2@gmail.com](mailto:hsyncalti2@gmail.com) · [karakedidub.com](https://karakedidub.com)

</div>
