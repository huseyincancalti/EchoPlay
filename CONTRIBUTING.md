# Contributing

Extending EchoPlay is as easy as dropping in a file — no coding required.

## 🌐 Add a language

1. In the player, open **Settings → Language → Open language folder** (this is `script-opts/`).
2. Copy `echoplay-en.json` and rename it `echoplay-<code>.json` (e.g. `echoplay-de.json` for German).
3. Translate the values. Set `"lang_name"` to the language's own name (e.g. `"Deutsch"`).
4. Save and restart mpv — the new language appears in the **Language** menu automatically.
5. To make it official, open a pull request adding the file under `mpv/script-opts/`.

## 🎨 Add a theme

Built-in **Dark / Light + accent color** live in **Settings → Theme**. A theme *pack* is for a
fuller custom look — it can override any uosc option, not just the accent.

1. Open **Settings → Theme → Open theme folder** (this is `themes/`).
2. Copy `amoled.conf` and rename it `<name>.conf`.
3. The first `# Comment` line is the name shown in the menu. Every other line is a uosc option:
   `key=value` — for example `color=foreground=ff5500`, `border_radius=14`, `opacity=menu=0.85`.
   Full option list: <https://github.com/tomasklaen/uosc>.
4. Save — the theme shows up under **Custom Themes** and applies live when selected.
5. Share it with a pull request under `mpv/themes/`.

## Project structure

| Path | Contents |
|------|----------|
| `mpv/scripts/echoplay-audio/` | EchoPlay's only original code — `main.lua` (mixer + menus + theming + shortcuts) + `i18n.lua`/`quality.lua`/`resume.lua`/`update_check.lua` (`version.lua` is a build-time placeholder, see release.yml) |
| `mpv/script-opts/echoplay-*.json` | Language packs |
| `mpv/themes/*.conf` | Theme packs |
| `mpv/mpv.conf`, `mpv/input.conf` | Player + shortcut settings |
| `install.ps1` | Installer (downloads uosc/thumbfast/memo, icon, branding, association) |

uosc / thumbfast / memo are downloaded from upstream — don't modify them here; contribute to their
own repositories instead. EchoPlay stays as close to upstream mpv as possible.

## Testing

Every push and pull request runs [`.github/workflows/ci.yml`](.github/workflows/ci.yml)
automatically: `luacheck` against `mpv/scripts/echoplay-audio/`, a plain-Lua unit test suite
([`tests/run.lua`](tests/run.lua) - no mpv process needed, run locally with `lua5.1 tests/run.lua`)
covering the pure logic that's easy to get subtly wrong (e.g. quality.lua's auto-tier stepping
thresholds), and a headless mpv smoke test that loads a synthetic clip and fails the build if
any Lua error shows up in the log. Changes to the Lua script should keep all three green - if you
add non-trivial pure logic, add a case to `tests/run.lua` too.
