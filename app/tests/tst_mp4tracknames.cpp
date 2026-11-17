#include <QtTest>
#include <QtEndian>
#include "Mp4TrackNames.h"

namespace {
QByteArray box(const char *type, const QByteArray &payload)
{
    QByteArray size(4, '\0');
    qToBigEndian<quint32>(quint32(8 + payload.size()), size.data());
    return size + QByteArray(type, 4) + payload;
}
// A trak with a handler and, when given, a name in its user data (the way NVIDIA's recorder writes it).
QByteArray trak(const char *handler, const QByteArray &name)
{
    const QByteArray hdlr = box("hdlr", QByteArray(8, '\0') + QByteArray(handler, 4) + QByteArray(12, '\0') + '\0');
    QByteArray body = box("tkhd", QByteArray(84, '\0')) + box("mdia", box("mdhd", QByteArray(24, '\0')) + hdlr);
    if (!name.isNull()) body += box("udta", box("name", name));
    return box("trak", body);
}
} // namespace

class TestMp4TrackNames : public QObject
{
    Q_OBJECT
private slots:
    void namesInTrackOrderPerKind()
    {
        const QByteArray moov = box("mvhd", QByteArray(100, '\0')) + trak("vide", "Video") + trak("soun", "System sounds")
                              + trak("soun", "Microphone") + trak("soun", QByteArray()) + trak("sbtl", "English SDH");
        const Mp4TrackNames names = Mp4TrackNames::parseMoov(moov);
        QCOMPARE(names.audio, QStringList({"System sounds", "Microphone", ""}));
        QCOMPARE(names.subtitles, QStringList({"English SDH"}));
    }
    void moovAtTheEndOfAFile()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write(box("ftyp", "isom") + box("mdat", QByteArray(100000, 'x')) + box("moov", trak("soun", QByteArray("Game\0", 5))));
        file.close();
        QCOMPARE(Mp4TrackNames::read(file.fileName()).audio, QStringList({"Game"}));
    }
    void notAnMp4()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write("\x1A\x45\xDF\xA3 matroska, not mp4");
        file.close();
        QVERIFY(Mp4TrackNames::read(file.fileName()).audio.isEmpty());
        QVERIFY(Mp4TrackNames::read("no such file").audio.isEmpty());
        QVERIFY(Mp4TrackNames::parseMoov(QByteArray("\xFF\xFF\xFF\xFFtrak", 8)).audio.isEmpty()); // a size past the end
    }
    // ECHOPLAY_NVIDIA_CLIP=<an NVIDIA App recording>: its real names.
    void realNvidiaRecording()
    {
        const QString clip = qEnvironmentVariable("ECHOPLAY_NVIDIA_CLIP");
        if (clip.isEmpty()) QSKIP("set ECHOPLAY_NVIDIA_CLIP to an NVIDIA App recording");
        QCOMPARE(Mp4TrackNames::read(clip).audio, QStringList({"System sounds", "Microphone"}));
    }
};

QTEST_GUILESS_MAIN(TestMp4TrackNames)
#include "tst_mp4tracknames.moc"
