<div align="center">

<img src="assets/logo.png" alt="EchoPlay" width="160" />

# EchoPlay

**A modern, friendly Windows video player built on mpv — for clips with more than one audio track.**

</div>

---

## What is EchoPlay?

EchoPlay is a curated **distribution of [mpv](https://mpv.io)** — *not a fork*. It wraps the
upstream engine with a modern [uosc](https://github.com/tomasklaen/uosc) interface and a single
small script, so you get a clean, YouTube-style player that adds the features mpv leaves to you.

It was born from one annoyance: **NVIDIA ShadowPlay / NVIDIA App** recordings ship with two audio
tracks (game/system sound + microphone). Most players force you to pick one, or re-encode the file
to mix them. EchoPlay mixes any combination of tracks **live, with no re-encode and no reloading
the video** — and remembers your choice for next time.

### Highlights

- 🎚️ **Live audio mixer** — toggle each track on/off, set per-track volume, and merge them in real
  time. A per-track **Mono** switch re-centers a channel-imbalanced track (e.g. a mic that only
  comes through the left speaker).
- 🔊 **Built-in loudness boost** — quiet recordings come out strong (`dynaudnorm`); turning the
  volume up is already a powerful boost.
- 🪟 **Modern UI** — YouTube-style timeline, hover thumbnails ([thumbfast](https://github.com/po5/thumbfast)),
  watch history & resume ([memo](https://github.com/po5/memo)), a right-edge volume slider, and clean
  keyboard feedback (your own volume / seek indicators, never the system's white bar).
- ⚙️ **One searchable Settings menu** — right-click (or `a`): mixer, fixed speed, end-of-video
  behavior, theme, language, and file/subtitle/playlist shortcuts, grouped into clear sections.
- 🎨 **Theming** — clean Dark / Light modes plus an accent color, switched live. Drop-in community
  theme & language packs (see below).
- 💾 **Persistent** — your mixer, mono, accent, language and speed choices survive restarts, and
  every file resumes where you left off.

## Download & Install

> Requires [mpv for Windows](https://mpv.io/installation/) (official build or `scoop install mpv`).

1. Download this repository (**Code → Download ZIP**, or `git clone`) and unzip it.
2. Open **PowerShell** in the EchoPlay folder and run:

   ```powershell
   powershell -ExecutionPolicy Bypass -File install.ps1
   ```

   The installer copies the EchoPlay config into mpv's config directory, downloads **uosc /
   thumbfast / memo**, embeds the EchoPlay logo and name into `mpv.exe`, and registers EchoPlay in
   the Windows *"Open with"* list. The target is detected automatically (scoop's `portable_config`,
   otherwise `%APPDATA%\mpv`); override with `-ConfigDir "C:\path"`, or `-Portable` for a portable build.

3. To make it your default player: right-click any video → **Open with → EchoPlay → Always**.

That's it — open a video, right-click (or press `a`) for **Settings**.

### Keyboard shortcuts

| Key | Action |
|-----|--------|
| `↑` / `↓` | Volume up / down (hold to ramp) |
| `←` / `→` | Seek 10s back / forward |
| `m` | Mute |
| `g` | Toggle fixed speed (factor set in the menu) |
| `Ctrl`+`1/2/3` | Toggle audio track 1 / 2 / 3 |
| Right-click / `a` | Settings menu |

---

## For the curious: how it works

EchoPlay is deliberately thin. The only original code is one Lua script
([`mpv/scripts/echoplay-audio.lua`](mpv/scripts/echoplay-audio.lua)); everything else is upstream
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
| [`mpv/scripts/echoplay-audio.lua`](mpv/scripts/echoplay-audio.lua) | EchoPlay's only original code — mixer, menus, theming |
| [`mpv/script-opts/echoplay-*.json`](mpv/script-opts) | Language packs |
| [`mpv/themes/*.conf`](mpv/themes) | Community theme packs |
| [`mpv/mpv.conf`](mpv/mpv.conf), [`mpv/input.conf`](mpv/input.conf) | Player settings & shortcuts |
| [`install.ps1`](install.ps1) | Installer (fetches uosc/thumbfast/memo, icon, branding, association) |

## Credits & license

EchoPlay's own code (`echoplay-audio.lua` + config/JSON/theme files) is **MIT** — see
[LICENSE](LICENSE). It downloads and bundles, under their own licenses:
[uosc](https://github.com/tomasklaen/uosc) (GPL-3.0) ·
[thumbfast](https://github.com/po5/thumbfast) · [memo](https://github.com/po5/memo) ·
and of course [mpv](https://mpv.io) itself.

---

<div align="center">

Made by **Hüseyin Can ÇALTI**
[hsyncalti2@gmail.com](mailto:hsyncalti2@gmail.com) · [karakedidub.com](https://karakedidub.com)

</div>
