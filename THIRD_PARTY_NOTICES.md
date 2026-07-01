# Third-Party Notices

EchoPlay's own code — `mpv/scripts/echoplay-audio.lua`, `mpv/mpv.conf`,
`mpv/input.conf`, the language packs under `mpv/script-opts/`, and the theme
packs under `mpv/themes/` — is **MIT-licensed**. See [`LICENSE`](LICENSE) in
this repository.

Every EchoPlay download (Windows installer, macOS `.app`/DMG, Linux AppImage)
bundles the upstream components listed below, each under its own license.
Full license texts are in [`licenses/`](licenses/).

## mpv

<https://mpv.io> — the player engine itself. mpv is dual-licensed under the
GNU General Public License v2.0-or-later and the GNU Lesser General Public
License v2.1-or-later; which one applies depends on the specific build's
compile-time configuration. EchoPlay redistributes upstream's own official
builds unmodified (only renamed/re-branded, engine code untouched):

- Windows: [zhongfly/mpv-winbuild](https://github.com/zhongfly/mpv-winbuild)
- macOS: [mpv-player/mpv](https://github.com/mpv-player/mpv)'s own CI-published `.app` bundle
- Linux: the distro-packaged `mpv` binary (Ubuntu)

See [`licenses/GPL-2.0.txt`](licenses/GPL-2.0.txt) and
[`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt).

## uosc

<https://github.com/tomasklaen/uosc> — the on-screen UI (timeline, menus,
thumbnails layout). Copyright (c) tomasklaen and contributors.
**License: GNU Lesser General Public License v2.1** (`LICENSE.LGPL` in
uosc's own repository).

See [`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt).

## memo

<https://github.com/po5/memo> — watch history and resume-from-last-position.
Copyright (c) po5 and contributors.
**License: GNU General Public License v3.0**.

See [`licenses/GPL-3.0.txt`](licenses/GPL-3.0.txt).

## thumbfast

<https://github.com/po5/thumbfast> — hover-scrub timeline thumbnails.
Copyright (c) po5 and contributors.
**License: Mozilla Public License 2.0**.

See [`licenses/MPL-2.0.txt`](licenses/MPL-2.0.txt).

---

None of the above components' source code has been modified by EchoPlay —
they are fetched from their own upstream releases at build/install time and
bundled as-is alongside EchoPlay's own MIT-licensed configuration and Lua
script. Source for each is available at the URLs above.
