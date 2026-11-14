# Language persistence testing

Run the unit suite from the repository root with `lua tests/run.lua`.
`bash tools/check.sh v1` also runs lint and syntax checks when those tools are installed.

On Windows with EchoPlay installed, no standalone Lua installation is needed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test-language.ps1
```

Use `-InstallDir 'C:\path\to\EchoPlay'` for a non-default installation. This runs the
same unit suite using mpv's Lua runtime, tests real config replacement and creation,
then boots the full installed configuration with the patched scripts on a synthetic
clip. Logs and the isolated configuration are under `.out/`. The installed files
are never changed. The smoke run is bounded to eight seconds because EchoPlay can
restore its keep-open setting after mpv starts. This does not replace luacheck.

## Manual Windows regression

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test-language.ps1 -Preview
```

1. Right-click the preview player, choose Language, and select Turkish, then English.
   The menu text and active language indicator must agree immediately.
2. Inspect `.out/language-preview/script-opts/echoplay.conf`: its active `language=`
   entries should now be `en`; comments and unrelated settings must be unchanged.
3. Close the preview and rerun the same command. English text and selection should
   persist. Repeat in the other direction.
4. Close the preview, set the copied config to `language=tr` and the copied
   `.out/language-preview/echoplay-state.json` language to `en`. Reopen the preview:
   both the displayed language and the indicator must be English. Saved state is
   authoritative; the config supplies the default when state has no language.
5. Close the preview, remove only the copied `echoplay.conf`, reopen and select a
   language. The config should be recreated containing the selected language.
6. With the preview closed, mark its copied config read-only. Reopen and switch
   language: the interface changes, a warning appears, and the original config is
   intact. Clear read-only afterwards. Saved state should still restore the choice.

Missing or unreadable translation packs must not change the current selection.
An unavailable startup language falls back to Turkish with a matching indicator.
If state itself cannot be saved, a warning explains that language may reset.
Automated failure injection covers read, write, close, temporary-file and replacement
errors without changing permissions on the real installation.

On Windows, config replacement uses Windows PowerShell's `System.IO.File.Replace`
(or `Move` for a missing destination) after a checked temporary-file write. A blocked
PowerShell process or replacement failure is reported; no destructive overwrite
fallback is attempted. Existing state persistence remains the restart fallback.
