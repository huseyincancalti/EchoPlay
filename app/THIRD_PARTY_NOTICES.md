# Third-Party Notices (EchoPlay 2)

EchoPlay 2's own code (`app/` in <https://github.com/huseyincancalti/EchoPlay>) is **MIT-licensed**: see
`LICENSE`. The Windows installer also ships the upstream components below, each under its own license,
unmodified and dynamically linked (each is a separate DLL that can be replaced). License texts are in
`licenses/`.

Because libmpv (below) is a GPL build, the EchoPlay 2 program as distributed, taken as a whole, is
covered by the **GNU General Public License v3** (`licenses/GPL-3.0.txt`); EchoPlay's own source stays
MIT and may be reused under MIT on its own.

## libmpv (mpv, with FFmpeg and the libraries it is built with)

<https://mpv.io> - the playback engine (`libmpv-2.dll`). EchoPlay ships the developer build from
[zhongfly/mpv-winbuild](https://github.com/zhongfly/mpv-winbuild), release `2026-10-29-cf764c61d1`
(mpv commit `cf764c61d1`):
<https://github.com/zhongfly/mpv-winbuild/releases/tag/2026-10-29-cf764c61d1>. That build links FFmpeg and
other libraries statically and includes GPL components, so this `libmpv-2.dll` is licensed under the
**GNU General Public License, version 2 or later** (`licenses/GPL-2.0.txt`, `licenses/GPL-3.0.txt`). Source:
mpv at <https://github.com/mpv-player/mpv>, the build scripts and the exact library versions at the release
link above.

## Qt 6

<https://www.qt.io> - the user interface framework (`Qt6*.dll`, `qml/`, `plugins/`), version 6.10.3, used
under the **GNU Lesser General Public License v3** (`licenses/LGPL-3.0.txt`, which builds on
`licenses/GPL-3.0.txt`). Source: <https://download.qt.io/archive/qt/6.10/6.10.3/single/> and
<https://code.qt.io>. Qt is not modified; it is linked dynamically, so the Qt DLLs can be replaced with
other builds of the same version.

## MinGW-w64 runtime

The C++ runtime of the compiler EchoPlay is built with (MinGW-w64, GCC 13.1):
- `libstdc++-6.dll`, `libgcc_s_seh-1.dll`: GCC runtime libraries, GNU General Public License v3 with the
  GCC Runtime Library Exception 3.1 (`licenses/GPL-3.0.txt`,
  `licenses/GCC-runtime-library-exception-3.1.txt`). Source: <https://gcc.gnu.org>.
- `libwinpthread-1.dll`: mingw-w64 winpthreads, MIT-style license (`licenses/mingw-w64-winpthreads.txt`).
  Source: <https://www.mingw-w64.org>.

## Fonts

Embedded in `EchoPlay.exe`, unmodified:
- Geist, Manrope, Plus Jakarta Sans: SIL Open Font License 1.1 (`licenses/OFL-Geist.txt`,
  `licenses/OFL-Manrope.txt`, `licenses/OFL-PlusJakartaSans.txt`). Source: <https://github.com/google/fonts>.
- Material Symbols Rounded (the icons), cut down to the glyphs EchoPlay uses by `tools/make-fonts.sh`: Apache License 2.0
  (`licenses/Apache-2.0.txt`). Source: <https://github.com/google/material-design-icons>.

## Vulkan loader

`vulkan-1.dll`: the Khronos Vulkan loader, unmodified, from LunarG's Vulkan Runtime 1.4.363.0. libmpv needs
it to start; it is shipped for a Windows whose graphics driver did not install one. Apache License 2.0
(`licenses/Apache-2.0.txt`). Source: <https://github.com/KhronosGroup/Vulkan-Loader>, binaries from
<https://vulkan.lunarg.com>.

## Microsoft D3DCompiler

`D3Dcompiler_47.dll` (Direct3D shader compiler), redistributed as provided by Microsoft with the Windows
SDK, under Microsoft's redistribution terms.
