# EchoPlay: rules for external engines (opencode, Codex, agy, Cursor)

Read `CLAUDE.md` first; it is the single source of project rules. In short, for engine runs:

- Do only what your brief in `.motor/` asks. Touch no other files.
- Never commit, push, tag, or create releases. Never edit `CLAUDE.md`, `AGENTS.md`, `NOW.md`, `HANDOFF.md`.
- Never fork or patch mpv/libmpv, uosc, thumbfast or memo source.
- Keep code minimal: no speculative abstractions, no features beyond the brief.
- Everything you write in the repo is English.
- Before reporting done, run `bash tools/check.sh` and paste its last line into your report.
