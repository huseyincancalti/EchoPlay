# EchoPlay

Curated Windows/macOS/Linux **distribution of mpv** for dual-audio-track recordings
(NVIDIA ShadowPlay / NVIDIA App / OBS Studio clips with separate game+mic tracks). **Not a fork**
of mpv, uosc, thumbfast, or memo — those ship as unmodified upstream binaries/scripts. EchoPlay's
only original code is `mpv/scripts/echoplay-audio/` (Lua: `main.lua` mixer/menus/theming/shortcuts,
`i18n.lua`, `quality.lua`, `resume.lua`, `update_check.lua`; `version.lua` is a build-time-only
placeholder — see "Update check" below) plus `mpv/mpv.conf`, `mpv/input.conf`, language/theme packs,
and the installer.

## Hard rules

- **Never fork or modify uosc/thumbfast/memo/mpv source.** Contribute upstream instead. If a uosc
  limitation blocks a request (e.g. no menu size/position API), say so plainly rather than faking
  a partial solution or forking to get it.
- **Never create a git tag or publish a GitHub Release without explicit prior user approval** —
  even after pushing to `main`.
- **SOLID / minimal code.** No speculative abstractions, no features beyond what's asked, no
  half-finished implementations. Three similar lines beat a premature helper.
- **The single-EXE installer must stay plug-and-play.** No added prerequisites, no manual config
  steps for the end user.
- Think before coding: understand the actual root cause first (see the picture-quality stutter fix
  in git history for the standard this project holds — root-caused via verbose mpv logs and exact
  timestamps, not guessed at). Prefer surgical, verifiable changes over broad rewrites.

## Commands

```bash
lua tests/run.lua                                   # pure-Lua unit tests, no mpv needed
luacheck mpv/scripts/echoplay-audio/*.lua            # lint
luac -p mpv/scripts/echoplay-audio/<file>.lua        # syntax check
```
CI (`ci.yml`) runs all of the above plus a headless mpv smoke test on every push/PR — keep all
green. Add a `tests/run.lua` case for any new non-trivial pure logic (see `quality.lua`'s auto-tier
stepping for the pattern: mock `mp`/`mp.utils` live in `tests/mocks/`).

## Structure

| Path | Contents |
|------|----------|
| `mpv/scripts/echoplay-audio/` | All original Lua code |
| `mpv/script-opts/echoplay-*.json` | Language packs (see `CONTRIBUTING.md` to add one) |
| `mpv/themes/*.conf` | Theme packs |
| `mpv/mpv.conf`, `mpv/input.conf` | Player + shortcut defaults |
| `installer/windows/EchoPlay.iss` | Inno Setup script |
| `install.ps1` / `install.sh` | Non-Windows / dev installers |
| `tests/` | Pure-Lua unit tests + mp/mp.utils mocks |

## State persistence

User choices (mixer, mono/stereo, volume, end-of-video mode, speed, theme, language, shortcuts)
persist across files and process restarts via `echoplay-state.json`, loaded once at script start
and applied through explicit `apply_*` functions (e.g. `apply_end`, mirroring `saved_volume`).
**Any new user-facing setting must follow this same load → apply-at-start → save-on-change pattern**
— a setting that only lives in mpv's in-process properties silently reverts on the next "Open with".

## Known constraints (don't relitigate without new evidence)

- uosc's `MenuData` schema has no size/position/orientation override — a portrait-aware or
  screen-fit settings menu is not achievable without forking uosc.
- `directory-mode=ignore` in `mpv.conf` is required — without it, a subfolder next to the opened
  file gets added to the auto-built playlist as its own entry and silently gets "played" once real
  videos are exhausted.
- The first `apply-profile` switch to a picture-quality tier in a fresh mpv process costs ~1-2s
  (GPU driver texture creation), never on repeat switches — handled by a one-time warm-up pass in
  `quality.lua`'s `M.apply_initial()`, not by removing the tier-switching feature.

## Update check

`update_check.lua` compares `version.lua` (a placeholder committed as `"0.0.0-dev"`) against
GitHub's latest release tag via `curl` (present by default on Windows/macOS/Linux — no added
prerequisite), at most once every 24h. `release.yml` overwrites `version.lua`'s placeholder with
the real tag on every platform (`Embed version into script` step, one per build job) right after
resolving `VERSION` and before the `mpv/` config folder gets copied into the build output — if a
new platform build job is ever added, it needs this same step or its builds will never detect
updates. The `0.0.0-dev` placeholder is a deliberate no-op guard (see `M.init`): a raw git
checkout must never fire the network check.

## Ecosystem tooling policy

This project deliberately does not install third-party Claude Code skill/agent frameworks beyond
what ships with the user's own global config. Evaluated and rejected as of 2026-07: multi-agent
orchestration frameworks, "virtual team" skill packs, and hosted memory services — all built for
team/enterprise scale and in direct tension with the minimal-tooling rule above. Re-evaluate
case-by-case only if a specific, concrete EchoPlay task needs it — not because a tool trends.
