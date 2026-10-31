# EchoPlay

Video player for dual-audio-track recordings (NVIDIA ShadowPlay / NVIDIA App / OBS Studio clips with
separate game + mic tracks): play any mix of the tracks live, without reopening the video.

Two tracks live in this repo:
- **v1 (shipping, maintenance only)**: a curated distribution of mpv + uosc/thumbfast/memo with our Lua
  script (`mpv/`, `installer/`, `tests/`). Bug fixes only, no new features (maintainer, 2026-10-29).
- **v2 (in development)**: our own player app, Qt 6 Quick/QML UI + C++ backend on libmpv (`app/`; the
  spike lives on branch `spike/qt-libmpv` until the MVP starts). Replaces v1 when ready: same repo, same
  GitHub releases, same update check. Windows first;
  Linux and macOS build in CI from the start and get real testing later.

## Start of every session
1. Read `NOW.md`: the only plan file (goal, decisions, phases, next steps). If `HANDOFF.md` exists, read it.
2. Chat with the maintainer in short, plain Turkish. Everything in the repo is English (code, docs, UI, commits).

## Session and context handoff
- At a phase end, or at about 70% context (measure with `mcp__ccd_session_mgmt__get_usage`): overwrite
  `HANDOFF.md` (state with evidence, broken or unchecked, next step, gotchas), update `NOW.md`, then start
  the next session yourself with "Read HANDOFF.md and continue." and archive this one. If the app refuses
  (Remote Control), tell the maintainer in one line: "/clear yaz, sonra şunu gönder: HANDOFF.md'yi oku ve devam et."
- `NOW.md`, `HANDOFF.md`, `.motor/`, `.out/` and `opencode.json` are local only (git-ignored). Never commit them.

## Checks (evidence before "done")
`bash tools/check.sh [v1|v2|all]` prints one line per step; the full log goes to `.out/`. Exit 0 = passed,
1 = a step failed, 3 = nothing could run. A report is a claim; this output, or what the maintainer saw, is
the evidence. Any check or measurement that will likely be run again (benchmarks, platform tests,
screenshots) is written as a script in `tools/` from the start (maintainer, 2026-10-29), not first done by
hand. One-off investigations stay manual.

## Work split (save Claude credits)
- The main session writes small changes itself (about 20 lines or one file). Bigger pieces go to one
  external engine or one subagent with a short brief.
- External engines first for chores. Briefs and reports go in `.motor/`:
  - code chore with a clear brief: `opencode run -m evren/glm-5.3 --agent build "$(cat .motor/brief-X.md)"`
  - research and summaries: `agy` (Gemini flash)
  - second-opinion review: `codex exec -s read-only "<what to review>"`
  - backup: Cursor CLI
  Check every engine result with `git diff` + `tools/check.sh` before keeping it.
- Claude keeps: UI/UX design and the QML look, visual checks with screenshots, the mixer's filter graph,
  and state persistence.
- No third-party Claude Code skill/agent frameworks; the engines above are the maintainer's own CLIs.
  Exception approved by the maintainer (2026-10-29): `ffmpeg-skill` (kajisho5, MIT, installed for Claude,
  Codex and Cursor) for ad-hoc video work. Repo scripts in `tools/` call ffmpeg directly, never the skill.

## Hard rules
- Never fork or patch mpv/libmpv, uosc, thumbfast or memo source. v2 links upstream libmpv; v1 ships the
  upstream scripts unmodified. If an upstream limit blocks a request, say so plainly instead of faking it.
- Never create a git tag or publish a GitHub Release without explicit prior approval from the maintainer.
- SOLID / minimal code. No speculative abstractions, no features beyond what's asked, no half-finished work.
- The single-EXE installer stays plug-and-play: no added prerequisites, no manual steps for the user.
- Root cause first, measured (verbose mpv logs, exact timestamps), then a surgical fix.
- Languages: one JSON file per language. Adding a language = dropping one file, no rebuild.
- Theme: one token file (colors, radii, blur, fonts, opacity). Editing it changes the look without code.
  The limits are deliberate: tokens only, like a constrained userChrome.css.

## v2 decisions (2026-10-29; the maintainer left the tech choice to Claude)
- Qt 6 Quick/QML + C++ backend, CMake. Video goes through the libmpv render API inside the QML scene so
  glass panels can blur the live video (a webview or a native child window cannot). References:
  mpv-examples `libmpv/qml`, Haruna, mpc-qt.
- Mixer: v1's proven `lavfi-complex` technique, ported to C++.
- UI/UX is designed first in v0 from `docs/design/v0-brief.md`; the maintainer picks; then QML.

### v2 commands
```bash
bash tools/setup-dev.sh                 # one-time: Qt (MinGW kit), MinGW, CMake, Ninja, libmpv into ~/.echoplay-dev
bash tools/check.sh v2                  # configure + build + ctest (sources .out/devenv.sh)
bash tools/make-clips.sh <recording> [start] [len]   # test clips in .out/clips: 1080p cut + 4K60 upscale, all tracks
RUNS=3 TOGGLE=1 bash tools/bench.sh <seconds> <videos>  # v1 engine vs v2: startup, dropped frames, live mixer
bash tools/shot-compare.sh <video> <seconds>         # same paused frame from v1 and v2, SSIM/PSNR
```
Dev tools live in `~/.echoplay-dev`, not `%LOCALAPPDATA%`: the Claude desktop app redirects new folders
there into its own package cache, invisible to other programs.

## v1 reference (true for the code under `mpv/`)

### Commands
```bash
lua tests/run.lua                                   # pure-Lua unit tests, no mpv needed
luacheck mpv/scripts/echoplay-audio/*.lua            # lint
luac -p mpv/scripts/echoplay-audio/*.lua             # syntax check
```
`tools/check.sh v1` runs all three. CI (`ci.yml`) also runs a headless mpv smoke test on every push/PR;
keep it green. Add a `tests/run.lua` case for any new non-trivial pure logic (pattern: `quality.lua`'s
auto-tier stepping; mock `mp`/`mp.utils` live in `tests/mocks/`).

### Structure
| Path | Contents |
|------|----------|
| `mpv/scripts/echoplay-audio/` | All original Lua code (`main.lua`, `i18n.lua`, `quality.lua`, `resume.lua`, `update_check.lua`, `version.lua`) |
| `mpv/script-opts/echoplay-*.json` | Language packs (see `CONTRIBUTING.md` to add one) |
| `mpv/themes/*.conf` | Theme packs |
| `mpv/mpv.conf`, `mpv/input.conf` | Player + shortcut defaults |
| `installer/windows/EchoPlay.iss` | Inno Setup script |
| `install.ps1` / `install.sh` | Non-Windows / dev installers |
| `tests/` | Pure-Lua unit tests + mp/mp.utils mocks |
| `tools/` | Repeatable checks (`check.sh`) |

### State persistence
User choices (mixer, mono/stereo, volume, end-of-video mode, speed, theme, language, shortcuts) persist
across files and restarts via `echoplay-state.json`, loaded once at script start and applied through
explicit `apply_*` functions (e.g. `apply_end`, mirroring `saved_volume`). **Any new user-facing setting
must follow this load → apply-at-start → save-on-change pattern**; a setting that only lives in mpv's
in-process properties silently reverts on the next "Open with". v2 keeps the same rule.

### Known constraints (don't relitigate without new evidence)
- uosc's `MenuData` schema has no size/position/orientation override, no tabs, no backdrop blur, no
  popover anchored to its button. This is why v2 exists.
- `directory-mode=ignore` in `mpv.conf` is required: without it a subfolder next to the opened file joins
  the auto-built playlist and silently gets "played" once the real videos are exhausted.
- The first `apply-profile` switch to a picture-quality tier in a fresh mpv process costs ~1-2 s (GPU
  driver texture creation), never on repeat switches; handled by the warm-up pass in `quality.lua`'s
  `M.apply_initial()`.
- `osd-font=Segoe UI Variable Display` is not found by libass: the uosc UI falls back to Arial (`-v` log:
  `fontselect: (Segoe UI Variable Display, 400, 0) -> ArialMT`).

### Update check
`update_check.lua` compares `version.lua` (placeholder `"0.0.0-dev"`) against GitHub's latest release tag
via `curl`, at most once every 24 h. `release.yml` overwrites the placeholder with the real tag on every
platform (`Embed version into script` step, one per build job) before `mpv/` is copied into the build
output; a new platform build job needs the same step. The placeholder is a deliberate no-op guard
(`M.init`): a raw git checkout never fires the network check.
