# Vendored upstream libmpv header

`mpv/render_d3d11.h` is copied unmodified from mpv-player/mpv
(`include/mpv/render_d3d11.h`, commit 4255130de410e0cbdab694b951085409d466b704, ISC license, header kept).
The libmpv build we link (zhongfly/mpv-winbuild) already contains the D3D11 render backend and ships the
matching enums in `render.h`, but its dev package does not include this header yet. Delete this folder
once the dev package ships it.
