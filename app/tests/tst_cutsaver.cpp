#include <QtTest>
#include <clocale>
#include "CutSaver.h"

// Saves parts of the generated 10 s clip with two audio tracks (tests/CMakeLists.txt, clipinfo-clips) through the
// hidden libmpv encoder, then reads the result back with ffprobe.

namespace {

constexpr int Slow = 30000; // ms: the longest a 3 s encode may take on a slow runner
const QString Mix = "[aid1]volume=1.00[ev1];[aid2]volume=1.00[ev2];[ev1][ev2]amix=inputs=2:normalize=0[ao]";

// "codec_type,codec_name" per stream, and the duration in seconds.
QStringList streams(const QString &file, double *duration)
{
    QProcess probe;
    probe.start(ECHOPLAY_FFPROBE, {"-v", "error", "-show_entries", "stream=codec_type,codec_name:format=duration",
                                   "-of", "csv=p=0", file});
    probe.waitForFinished(Slow);
    QStringList lines = QString::fromUtf8(probe.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    for (QString &line : lines) line = line.trimmed();
    *duration = lines.isEmpty() ? 0 : lines.takeLast().toDouble(); // format comes last
    lines.sort(); // with a graph mpv writes the audio stream first; the order does not matter to players
    return lines;
}

} // namespace

class TestCutSaver : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_clip;

private slots:
    void initTestCase()
    {
        std::setlocale(LC_NUMERIC, "C"); // libmpv requires it
        QVERIFY2(QFileInfo::exists(ECHOPLAY_CLIPINFO_TWO), "the clip is made by ctest's clipinfo-clips step");
        m_clip = m_dir.filePath(QString::fromUtf8("maç özeti.mp4")); // a Turkish name has to reach libmpv as UTF-8
        QVERIFY(QFile::copy(ECHOPLAY_CLIPINFO_TWO, m_clip));
    }

    void outputPath()
    {
        const QString dir = m_dir.path();
        QCOMPARE(CutSaver::outputPath(dir + "/a.b.mp4", 5.9, 95), dir + "/a.b 0.05-1.35.mp4");
        QCOMPARE(CutSaver::outputPath(dir + "/x.mkv", 3725, 3726), dir + "/x 1.02.05-1.02.06.mp4");
        QFile taken(dir + "/y 0.01-0.02.mp4");
        QVERIFY(taken.open(QIODevice::WriteOnly));
        taken.close();
        QCOMPARE(CutSaver::outputPath(dir + "/y.mp4", 1, 2), dir + "/y 0.01-0.02 (2).mp4");
    }

    void savesTheMixAsOneTrack()
    {
        CutSaver saver(nullptr);
        QVERIFY(saver.save(m_clip, 2, 5, Mix, "no"));
        QCOMPARE(saver.state(), QString("saving"));
        QVERIFY(!saver.save(m_clip, 2, 5, Mix, "no")); // one at a time
        QTRY_VERIFY_WITH_TIMEOUT(saver.state() != "saving", Slow);
        QVERIFY2(saver.state() == "done", qPrintable(saver.error()));
        QCOMPARE(saver.progress(), 1.0);
        QCOMPARE(saver.output(), m_dir.filePath(QString::fromUtf8("maç özeti 0.02-0.05.mp4")));
        double duration = 0;
        const QStringList kinds = streams(saver.output(), &duration);
        QCOMPARE(kinds, (QStringList{"aac,audio", "h264,video"}));
        QVERIFY2(std::abs(duration - 3) < 0.15, qPrintable(QString::number(duration)));
        saver.dismiss();
        QCOMPARE(saver.state(), QString("idle"));
    }

    void savesOneTrackWithoutAGraph()
    {
        CutSaver saver(nullptr);
        QVERIFY(saver.save(m_clip, 0, 1, QString(), "2"));
        QTRY_VERIFY_WITH_TIMEOUT(saver.state() != "saving", Slow);
        QVERIFY2(saver.state() == "done", qPrintable(saver.error()));
        double duration = 0;
        QCOMPARE(streams(saver.output(), &duration), (QStringList{"aac,audio", "h264,video"}));
    }

    void cancelRemovesTheFile()
    {
        CutSaver saver(nullptr);
        QVERIFY(saver.save(m_clip, 0, 9, Mix, "no"));
        const QString output = saver.output();
        QTRY_VERIFY_WITH_TIMEOUT(saver.progress() > 0, Slow); // it really writes
        saver.cancel();
        QCOMPARE(saver.state(), QString("idle"));
        QVERIFY(!QFileInfo::exists(output));
    }

    void brokenFileFails()
    {
        const QString junk = m_dir.filePath("junk.mp4");
        QFile file(junk);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(4096, 'x'));
        file.close();
        CutSaver saver(nullptr);
        QVERIFY(saver.save(junk, 0, 1, QString(), "1"));
        QTRY_VERIFY_WITH_TIMEOUT(saver.state() != "saving", Slow);
        QCOMPARE(saver.state(), QString("failed"));
        QVERIFY(!saver.error().isEmpty());
        QVERIFY(!QFileInfo::exists(saver.output()));
    }
};

QTEST_GUILESS_MAIN(TestCutSaver)
#include "tst_cutsaver.moc"
