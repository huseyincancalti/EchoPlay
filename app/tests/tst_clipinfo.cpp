#include <QtTest>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <clocale>
#include "ClipInfo.h"

// The hidden libmpv player on two generated 10 s clips (tests/CMakeLists.txt): black for the first 0.4 s, then
// gray, a keyframe every 0.4 s; one has two audio tracks, one only video. The clips are copied under
// Turkish file names, which have to reach libmpv as UTF-8.

namespace {

constexpr int Slow = 20000; // ms: the longest a few reads may take

QImage pictureOf(const ClipInfo &clipInfo, const QString &path)
{
    const QString url = clipInfo.info(path).value("thumbnail").toString();
    return clipInfo.image(url.mid(url.lastIndexOf('/') + 1).toInt());
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

} // namespace

class TestClipInfo : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_two, m_video, m_third; // the clips; m_third is a second copy of the video-only one

private slots:
    void initTestCase()
    {
        std::setlocale(LC_NUMERIC, "C"); // libmpv requires it
        QVERIFY2(QFileInfo::exists(ECHOPLAY_CLIPINFO_TWO), "the clips are made by ctest's clipinfo-clips step");
        QVERIFY(m_dir.isValid());
        m_two = m_dir.filePath(QString(u"çift ses ışık.mp4"));
        m_video = m_dir.filePath(QString(u"yalnız görüntü.mp4"));
        m_third = m_dir.filePath(QString(u"üçüncü.mp4"));
        QVERIFY(QFile::copy(ECHOPLAY_CLIPINFO_TWO, m_two));
        QVERIFY(QFile::copy(ECHOPLAY_CLIPINFO_VIDEO, m_video));
        QVERIFY(QFile::copy(ECHOPLAY_CLIPINFO_VIDEO, m_third));
    }

    void readsDurationTracksAndPictureInQueueOrder()
    {
        ClipInfo c(nullptr);
        QSignalSpy changed(&c, &ClipInfo::changed);
        QVERIFY(c.info(m_two).isEmpty());
        QCOMPARE(c.revision(), 0);

        c.request({m_two, m_video});
        QVERIFY(changed.wait(Slow));
        QCOMPARE(c.revision(), 1);
        QVERIFY(!c.info(m_two).isEmpty()); // the first in the queue first
        QVERIFY(c.info(m_video).isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(c.revision(), 2, Slow);
        QCOMPARE(changed.count(), 2);

        const QVariantMap two = c.info(m_two);
        QVERIFY(qAbs(two["duration"].toDouble() - 10) < 0.5);
        QCOMPARE(two["audioTracks"].toInt(), 2);
        QVERIFY(two["thumbnail"].toString().startsWith("image://clipinfo/"));
        const QImage picture = pictureOf(c, m_two);
        QCOMPARE(picture.width(), ClipInfo::ThumbWidth);
        QVERIFY(qAbs(picture.height() - 108) <= 1); // 640x360 as 192x108
        // The picture is the one at 10 % (gray), not the first one (black).
        const int gray = qRed(picture.pixel(picture.width() / 2, picture.height() / 2));
        QVERIFY2(gray > 100, qPrintable(QString("the picture is black: %1").arg(gray)));

        const QVariantMap video = c.info(m_video);
        QVERIFY(qAbs(video["duration"].toDouble() - 10) < 0.5);
        QCOMPARE(video["audioTracks"].toInt(), 0);
        QCOMPARE(pictureOf(c, m_video).width(), ClipInfo::ThumbWidth);
        QVERIFY(two["thumbnail"] != video["thumbnail"]);
    }

    // HDR10 red comes out red, not dull brown; the next, SDR clip gets the plain filter again (its gray stays gray).
    void hdrIsToneMapped()
    {
#ifndef ECHOPLAY_HDR_CLIP
        QSKIP("FFmpeg here has no zscale (zimg): HDR previews cannot be tone-mapped");
#else
        ClipInfo c(nullptr);
        c.request({ECHOPLAY_HDR_CLIP, m_video});
        QTRY_VERIFY_WITH_TIMEOUT(!pictureOf(c, m_video).isNull(), Slow);
        const QImage hdr = pictureOf(c, ECHOPLAY_HDR_CLIP);
        const QColor red = hdr.pixelColor(hdr.width() / 2, hdr.height() / 2);
        QVERIFY2(red.red() > 180 && red.green() < 40 && red.blue() < 40, qPrintable(red.name()));
        const QImage sdr = pictureOf(c, m_video);
        const QColor gray = sdr.pixelColor(sdr.width() / 2, sdr.height() / 2);
        QVERIFY2(qAbs(gray.red() - gray.green()) < 6 && gray.red() > 120, qPrintable(gray.name()));
#endif
    }

    void anAudioFileHasADurationAndNoPicture() // opening one makes a playlist of audio files
    {
        ClipInfo c(nullptr);
        c.request({QString(ECHOPLAY_CLIPINFO_AUDIO)});
        QTRY_COMPARE_WITH_TIMEOUT(c.revision(), 1, Slow);
        const QVariantMap audio = c.info(ECHOPLAY_CLIPINFO_AUDIO);
        QVERIFY(qAbs(audio["duration"].toDouble() - 10) < 0.5);
        QCOMPARE(audio["audioTracks"].toInt(), 1);
        QVERIFY(!audio.contains("thumbnail"));
    }

    void aClipThatCannotBeReadDoesNotStopTheQueue()
    {
        const QString garbage = m_dir.filePath("garbage.mp4"), empty = m_dir.filePath("empty.mp4"),
                      truncated = m_dir.filePath("truncated.mp4"), missing = m_dir.filePath("missing.mp4");
        QVERIFY(writeFile(garbage, "this is not a video, and it is long enough for a probe to read\n"));
        QVERIFY(writeFile(empty, {}));
        QFile whole(ECHOPLAY_CLIPINFO_TWO); // the index (moov) of an mp4 is at its end: this cannot be opened
        QVERIFY(whole.open(QIODevice::ReadOnly));
        QVERIFY(writeFile(truncated, whole.read(4096)));

        ClipInfo c(nullptr);
        c.request({missing, garbage, empty, truncated, m_two});
        QTRY_COMPARE_WITH_TIMEOUT(c.revision(), 5, Slow);
        for (const QString &path : {missing, garbage, empty, truncated}) {
            const QVariantMap bad = c.info(path);
            QVERIFY2(!bad.isEmpty(), qPrintable(path));
            QCOMPARE(bad["duration"].toDouble(), 0.0);
            QCOMPARE(bad["audioTracks"].toInt(), 0);
            QVERIFY(!bad.contains("thumbnail"));
        }
        QCOMPARE(c.info(m_two)["audioTracks"].toInt(), 2); // read after the four that failed

        c.request({missing, garbage}); // failed once: not tried again
        QTest::qWait(500);
        QCOMPARE(c.revision(), 5);
    }

    void aReadClipIsNotReadAgain()
    {
        ClipInfo c(nullptr);
        c.request({m_two});
        QTRY_COMPARE_WITH_TIMEOUT(c.revision(), 1, Slow);
        const QVariantMap before = c.info(m_two);
        QSignalSpy changed(&c, &ClipInfo::changed);

        c.request({m_two, m_two});
        c.request({});
        QVERIFY(!changed.wait(1000));
        QCOMPARE(c.revision(), 1);
        QCOMPARE(c.info(m_two), before);
    }

    void aNewRequestReplacesTheQueue()
    {
        ClipInfo c(nullptr);
        c.request({m_two, m_video}); // m_two is read at once
        c.request({m_two, m_third}); // m_video is dropped; m_two is being read, not queued again
        QTRY_COMPARE_WITH_TIMEOUT(c.revision(), 2, Slow);
        QSignalSpy changed(&c, &ClipInfo::changed);
        QVERIFY(!changed.wait(1000)); // nothing else is read
        QVERIFY(!c.info(m_two).isEmpty());
        QVERIFY(!c.info(m_third).isEmpty());
        QVERIFY(c.info(m_video).isEmpty());
    }

    void readsAgainAfterTheFileWasClosed()
    {
        ClipInfo c(nullptr);
        c.request({m_two});
        QTRY_COMPARE_WITH_TIMEOUT(c.revision(), 1, Slow);
        QTest::qWait(ClipInfo::IdleMs + 500); // the hidden player closed the file
        c.request({m_video});
        QTRY_COMPARE_WITH_TIMEOUT(c.revision(), 2, Slow);
        QCOMPARE(c.info(m_video)["audioTracks"].toInt(), 0);
        QVERIFY(!pictureOf(c, m_video).isNull());
    }

    void theImageProviderServesThePicture()
    {
        ClipInfo c(nullptr);
        ClipInfo::setInstance(&c);
        QQmlEngine engine;
        QCOMPARE(ClipInfo::create(&engine, nullptr), &c);
        auto *provider = static_cast<QQuickImageProvider *>(engine.imageProvider("clipinfo"));
        QVERIFY(provider);

        c.request({m_two});
        QTRY_COMPARE_WITH_TIMEOUT(c.revision(), 1, Slow);
        const QString url = c.info(m_two)["thumbnail"].toString();
        QSize size;
        const QImage served = provider->requestImage(url.mid(url.lastIndexOf('/') + 1), &size, {});
        QCOMPARE(served, pictureOf(c, m_two));
        QCOMPARE(size, served.size());
        QVERIFY(provider->requestImage("999", &size, {}).isNull()); // an id nobody has
    }
};

QTEST_GUILESS_MAIN(TestClipInfo)
#include "tst_clipinfo.moc"
