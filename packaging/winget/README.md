# winget manifest

`HuseyinCanCalti.EchoPlay/1.4.0/` is a ready-to-submit [winget-pkgs](https://github.com/microsoft/winget-pkgs)
manifest for `EchoPlay-Setup.exe` from the [v1.4.0 release](https://github.com/huseyincancalti/EchoPlay/releases/tag/v1.4.0)
(`InstallerSha256` is the real SHA-256 of that exact file, verified locally after download - not
copied from an unverified source).

**Not submitted yet.** Publishing it means opening a pull request against a Microsoft-owned repo
under this project's name - that's a call for the maintainer to make explicitly, not something to
do automatically as part of a release. To submit:

```bash
winget install wingetcreate  # or: https://github.com/microsoft/winget-create
wingetcreate submit --token <a GitHub PAT with public_repo scope> \
  packaging/winget/HuseyinCanCalti.EchoPlay/1.4.0
```

`wingetcreate` validates the manifest, forks `microsoft/winget-pkgs`, and opens the PR. Their CI
then runs installer validation automatically (silent install, publisher/signature checks) - review
any failure it reports before merging.

## Keeping it updated on future releases

Every new version needs a new `<version>/` folder with the same three files, `PackageVersion` and
`InstallerUrl`/`InstallerSha256` bumped to match. Either repeat the manual steps above, or - once
this package exists upstream - wire up
[vedantmgoyal2009/winget-releaser](https://github.com/vedantmgoyal2009/winget-releaser) as a step
in `release.yml` to open that PR automatically whenever a tag is pushed. Not added preemptively:
it only makes sense once the package identifier actually exists in winget-pkgs.

## macOS / Linux equivalents

Not built yet. The closest analogues would be a [Homebrew Cask](https://github.com/Homebrew/homebrew-cask)
for macOS (points at the `.dmg` release assets the same way this manifest points at the `.exe`)
and, for Linux, either registering the AppImage with an AppImage catalog or a Flatpak manifest -
each is its own separate submission process to a separate third-party repo, not something to set
up without the maintainer deciding it's worth the ongoing maintenance first.
