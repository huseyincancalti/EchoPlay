#pragma once

#include <QString>
#include <QStringList>

// Track names that MP4 and MOV files keep in each track's own user data (moov/trak/udta/name). NVIDIA ShadowPlay and the
// NVIDIA App write "System sounds" and "Microphone" there; VLC shows them. FFmpeg, and so mpv, does not read that box:
// without this every NVIDIA recording showed "Track 1" and "Track 2".
// The names come in the order mpv numbers the tracks of each kind (the order of the trak boxes); an empty string is a
// track without a name. Not an MP4 (or unreadable): both lists empty. Reads only box headers and the moov box.
struct Mp4TrackNames
{
    QStringList audio;
    QStringList subtitles;

    static Mp4TrackNames read(const QString &path);
    static Mp4TrackNames parseMoov(const QByteArray &moov); // the moov box's payload; for the unit test
};
