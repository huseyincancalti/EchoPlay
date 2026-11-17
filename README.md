<div align="center">

<img src="assets/logo.png" alt="EchoPlay — video player for NVIDIA ShadowPlay, NVIDIA App and OBS Studio recordings with separate game and mic audio tracks" width="160" />

# EchoPlay

**The video player for recordings with separate audio tracks: NVIDIA ShadowPlay, NVIDIA App and OBS Studio clips
with the game sound and your mic on their own tracks. Play either one, or both mixed, live.**

[![Latest release](https://img.shields.io/github/v/release/huseyincancalti/EchoPlay?label=download&color=ff7d26)](https://github.com/huseyincancalti/EchoPlay/releases/latest)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6)](#download--install)

<img src="docs/media/mixer.gif" alt="EchoPlay's mixer: the game sound and the microphone switched on and off while a Counter-Strike 2 clip plays" width="640" />

</div>

---

## What is EchoPlay?

**NVIDIA ShadowPlay**, the **NVIDIA App** and **OBS Studio** (with multi-track audio) save your clips with **two
separate audio tracks**: the game and your microphone. Most players make you pick one, or send you to an editor to
mix them. EchoPlay plays **any mix of the tracks, live, without reopening the video**, and remembers your choice for
the next clip.

EchoPlay 2 is its own player: a Qt Quick interface on [libmpv](https://mpv.io), the engine behind mpv.

### Highlights

- 🎚️ **The mixer, on the control bar.** One button shows what you hear (*System + Mic*). Each track has its own
  switch, volume up to 400% and a Mono switch for a mic that is only in one ear. Ctrl+1, Ctrl+2 from the keyboard.
- 🏷️ **Tracks say what they are:** the names your recorder wrote ("System sounds", "Microphone", "Commentary") and
  their language ("English", "Türkçe", "日本語"), for audio and subtitles.
- ⚡ **Starts in under a second** and plays 4K60 recordings without dropped frames. A second clip opens in the window
  that is already running.
- 🪟 **A modern look:** dark glass panels over the video, a floating control bar, light and dark themes, your accent
  color (or Windows'), your font. Every color and size lives in one theme file.
- 🎬 **Everything a player should have:** seek-bar previews, chapters, A-B loop, frame step, press and hold for fast
  playback, the folder as a playlist, resume where you left off, screenshots and copy frame, media keys and taskbar
  buttons, subtitles with delay, size and position, audio delay, picture adjustments, media info.
- 🌍 **7 languages:** English, Türkçe, Español, Русский, 简体中文, 日本語, 한국어. A new language is one file.
- 🔔 **Updates itself:** a small card when a new version is out; install now, or when you close the player.

![Settings over the playing video](docs/media/settings.png)

### EchoPlay vs. VLC, PotPlayer and mpv for clips with two audio tracks

| | VLC / PotPlayer | mpv | **EchoPlay** |
|---|---|---|---|
| Play one audio track | ✅ | ✅ | ✅ |
| **Mix several tracks together, live** | ❌ | ⚙️ by hand (`lavfi-complex`) | ✅ one click |
| Volume and mute per track | ❌ | ⚙️ by hand | ✅ |
| Fix a mic that is only in one ear (mono) | ❌ | ⚙️ by hand | ✅ one click |
| Remembers your mix | ❌ | ❌ | ✅ |
| Setup | Install | Install + config files | **One installer** |

## Download & Install

| System | Download |
|---|---|
| **Windows 10 / 11** | **EchoPlay-Setup.exe** from the [latest release](https://github.com/huseyincancalti/EchoPlay/releases/latest) (EchoPlay 2) |
| macOS (Apple silicon or Intel) | the `.dmg` from the [1.4.0 release](https://github.com/huseyincancalti/EchoPlay/releases/tag/v1.4.0) |
| Linux | the `.AppImage` from the [1.4.0 release](https://github.com/huseyincancalti/EchoPlay/releases/tag/v1.4.0) |

EchoPlay 2 is Windows only for now; macOS and Linux keep 1.4.0 until their version 2 is ready.

<details open>
<summary><strong>Windows</strong></summary>

1. Download **EchoPlay-Setup.exe** and run it. No admin rights and nothing else to install: it goes into your user
   folder.
2. To make it your default player: **Settings → Playback → Default player** in EchoPlay, or right-click a video →
   **Open with → EchoPlay → Always**.

Coming from 1.x? The setup replaces it in place. The 2.0.0 release notes have a one-time cleanup command if you want
every trace of the old version gone first.

</details>

<details>
<summary><strong>macOS</strong></summary>

1. Download **EchoPlay-\<version\>-arm64.dmg** (Apple Silicon: M1/M2/M3/M4) or
   **EchoPlay-\<version\>-x86_64.dmg** (Intel Mac) from the [1.4.0 release](https://github.com/huseyincancalti/EchoPlay/releases/tag/v1.4.0).
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

1. Download **EchoPlay-\<version\>-x86_64.AppImage** from the [1.4.0 release](https://github.com/huseyincancalti/EchoPlay/releases/tag/v1.4.0).
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

Every shortcut can be changed in **Settings → Shortcuts**.

| Key | Action |
|-----|--------|
| `Space` | Play / pause (a click on the picture too) |
| `←` / `→` | Seek back / forward (the step is a setting); `Shift` for a fine step |
| `↑` / `↓` | Volume up / down (up to 300%) |
| `M` | Mute |
| `Ctrl`+`1` / `2` / `3` | Track 1 / 2 / 3 on or off |
| `F` or double-click | Full screen |
| `G` | Fixed speed on or off; `S` / `D` slower / faster |
| `,` / `.` | Previous / next frame |
| `PgUp` / `PgDown` | Next / previous chapter |
| `L` | A-B loop |
| `<` / `>` | Previous / next file |
| `0`–`9` | Jump to 0%–90% of the clip |
| `F1` / `Shift`+`S` / `Ctrl`+`S` | Screenshot with subtitles / video only / the window |
| `Ctrl`+`C` | Copy the frame |
| `I` | Media info |
| `T` | Always on top |

### FAQ

**Why does my NVIDIA ShadowPlay / OBS recording have two audio tracks?**
NVIDIA's recorder and OBS Studio (with multi-track audio) save the game sound and your microphone as two tracks in the
same file instead of mixing them. Most players let you pick only one of them.

**Can I mix the two tracks without re-encoding the video?**
Yes, that's the point: EchoPlay rebuilds only the audio filter graph (`lavfi-complex`) while the video keeps playing.
Nothing is exported or re-encoded.

**Why are my tracks called "System sounds" and "Microphone"?**
That's how the NVIDIA recorder names them inside the file. EchoPlay shows whatever name the recorder, or whoever made
the file, wrote; a track without a name is "Track 1", "Track 2".

---

## For the curious: how it works

| Path | What it is |
|------|------------|
| [`app/`](app/) | EchoPlay 2: C++ (`src/`) and Qt Quick (`qml/`) on libmpv's render API (Direct3D 11 on Windows) |
| [`app/lang/*.json`](app/lang) | One file per language: copy `en.json`, translate the values, set `lang_name`. No rebuild. |
| [`app/theme/theme.json`](app/theme/theme.json) | Every color, radius, blur and font, as tokens |
| [`installer/windows/v2/`](installer/windows/v2) | The Inno Setup script for `EchoPlay-Setup.exe` |
| [`tools/`](tools) | Build, test and measure: `check.sh`, `e2e.sh`, `package-v2.sh`, `bench.sh` |
| [`mpv/`](mpv), [`installer/`](installer) | EchoPlay 1.x (mpv + uosc + one Lua script), still the macOS and Linux build |

The mixer turns the enabled tracks into one filter graph and hands it to mpv while the clip plays:

```text
# two tracks enabled, track 2 boosted to 200% and forced mono:
[aid1]volume=1.00[ev1];
[aid2]volume=2.00,pan=stereo|c0=0.5*c0+0.5*c1|c1=0.5*c0+0.5*c1[ev2];
[ev1][ev2]amix=inputs=2:normalize=0[ao]
```

One track at its normal volume needs no filter at all (`aid=N`).

Translations and themes are welcome as pull requests; see [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits & license

EchoPlay's own code is **MIT** ([LICENSE](LICENSE)). EchoPlay 2 for Windows ships [libmpv](https://mpv.io) built with
GPL parts, so the Windows program as a whole is under the **GPL v3**; it also ships [Qt](https://www.qt.io) (LGPL v3)
and the Vulkan loader (Apache 2.0). Details and full texts: [app/THIRD_PARTY_NOTICES.md](app/THIRD_PARTY_NOTICES.md),
[`licenses/`](licenses/). EchoPlay 1.x bundles mpv, [uosc](https://github.com/tomasklaen/uosc),
[thumbfast](https://github.com/po5/thumbfast) and [memo](https://github.com/po5/memo):
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Film in the screenshots: *Sintel* © Blender Foundation, [durian.blender.org](https://durian.blender.org), CC BY 3.0.

---

<div align="center">

Made by **Hüseyin Can ÇALTI**
[hsyncalti2@gmail.com](mailto:hsyncalti2@gmail.com) · [karakedidub.com](https://karakedidub.com)

</div>
