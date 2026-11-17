#pragma once

#include <QByteArray>
#include <mpv/client.h>

// The hidden players (seek-bar previews, playlist thumbnails) convert frames in software, which does not tone-map:
// an HDR10 or HLG frame came out pale and grey next to the picture on screen, where the renderer tone-maps. For such a
// file they add FFmpeg's zscale + tonemap after their scale. Mobius came closest to the picture on screen (Jellyfin's
// HDR10 test clip at 10 s, 2026-11-17: PSNR 28.9 dB against the window, hable 23.0, none 16.2). SDR files keep the
// plain scale: the chain would dim their highlights.
namespace HdrTone {

// After the first frame (video-params exists from then on).
inline bool isHdr(mpv_handle *mpv)
{
    char *gamma = mpv_get_property_string(mpv, "video-params/gamma");
    const bool hdr = gamma && (qstrcmp(gamma, "pq") == 0 || qstrcmp(gamma, "hlg") == 0);
    mpv_free(gamma);
    return hdr;
}

// `scale`: the player's own scale filter, e.g. "scale=w=360:h=-2". Scaled first: the float tone-mapping then runs on
// a few hundred pixels per line, not on the whole frame. Even sizes: zscale refuses 4:2:0 frames with an odd side
// (the seek-bar preview's 360x203 gave no picture at all).
inline QByteArray filter(const QByteArray &scale)
{
    return "lavfi=[" + scale + ":force_divisible_by=2,zscale=t=linear:npl=100,format=gbrpf32le,zscale=p=bt709,tonemap=tonemap=mobius:desat=0,"
           "zscale=t=bt709:m=bt709:r=tv,format=yuv420p]";
}

} // namespace HdrTone
