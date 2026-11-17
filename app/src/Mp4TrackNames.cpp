#include "Mp4TrackNames.h"

#include <QFile>
#include <QtEndian>

namespace {

struct Box
{
    QByteArray type;
    QByteArray payload;
};

// The child boxes of a box's payload. A malformed size ends the list.
QList<Box> children(const QByteArray &data)
{
    QList<Box> boxes;
    qsizetype at = 0;
    while (at + 8 <= data.size()) {
        quint64 size = qFromBigEndian<quint32>(data.constData() + at);
        qsizetype header = 8;
        if (size == 1) {
            if (at + 16 > data.size()) break;
            size = qFromBigEndian<quint64>(data.constData() + at + 8);
            header = 16;
        } else if (size == 0) {
            size = quint64(data.size() - at);
        }
        if (size < quint64(header) || size > quint64(data.size() - at)) break;
        boxes.append({data.mid(at + 4, 4), data.mid(at + header, qsizetype(size) - header)});
        at += qsizetype(size);
    }
    return boxes;
}

QByteArray child(const QByteArray &data, const char *type)
{
    for (const Box &box : children(data))
        if (box.type == type) return box.payload;
    return {};
}

} // namespace

Mp4TrackNames Mp4TrackNames::parseMoov(const QByteArray &moov)
{
    Mp4TrackNames names;
    for (const Box &trak : children(moov)) {
        if (trak.type != "trak") continue;
        // hdlr: version and flags (4), pre_defined (4), handler_type (4).
        const QByteArray hdlr = child(child(trak.payload, "mdia"), "hdlr");
        const QByteArray kind = hdlr.mid(8, 4);
        QByteArray name = child(child(trak.payload, "udta"), "name");
        const qsizetype end = name.indexOf('\0');
        if (end >= 0) name.truncate(end);
        const QString text = QString::fromUtf8(name).trimmed();
        if (kind == "soun") names.audio.append(text);
        else if (kind == "sbtl" || kind == "text" || kind == "subt") names.subtitles.append(text);
    }
    return names;
}

Mp4TrackNames Mp4TrackNames::read(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    // Walk the top-level boxes by their headers: the moov box often sits at the end, after gigabytes of media.
    qint64 at = 0;
    const qint64 total = file.size();
    while (at + 8 <= total) {
        if (!file.seek(at)) return {};
        const QByteArray header = file.read(16);
        if (header.size() < 8) return {};
        quint64 size = qFromBigEndian<quint32>(header.constData());
        qint64 skip = 8;
        if (size == 1) {
            if (header.size() < 16) return {};
            size = qFromBigEndian<quint64>(header.constData() + 8);
            skip = 16;
        } else if (size == 0) {
            size = quint64(total - at);
        }
        if (size < quint64(skip) || size > quint64(total - at)) return {};
        const QByteArray type = header.mid(4, 4);
        if (at == 0 && type != "ftyp" && type != "moov" && type != "wide" && type != "free" && type != "mdat") return {}; // not MP4/MOV
        if (type == "moov") {
            if (size > 64ull * 1024 * 1024) return {}; // a real moov is a few MB at most
            file.seek(at + skip);
            return parseMoov(file.read(qint64(size) - skip));
        }
        at += qint64(size);
    }
    return {};
}
