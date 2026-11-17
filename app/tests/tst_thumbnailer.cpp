#include <QtTest>
#include <clocale>
#include "Thumbnailer.h"

// The hidden libmpv player on a generated 40 s clip (tests/CMakeLists.txt): flat gray, Y = 16 + 5 * second,
// a keyframe every 2 s. The gray of a picture tells which second of the clip it shows.

namespace {

int secondShown(const QImage &image)
{
    if (image.isNull()) return -1;
    const int gray = qRed(image.pixel(image.width() / 2, image.height() / 2));
    return qRound(gray / (5 * 255.0 / 219)); // limited-range Y step of 5 -> full-range RGB
}

} // namespace

class TestThumbnailer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        std::setlocale(LC_NUMERIC, "C"); // libmpv requires it
        QVERIFY2(QFileInfo::exists(ECHOPLAY_THUMB_CLIP), "the clip is made by ctest's thumbnailer-clip step");
    }

    void keyframeFirstThenExactOnRest()
    {
        Thumbnailer t(nullptr);
        QSignalSpy pictures(&t, &Thumbnailer::imageChanged);
        t.setFile(ECHOPLAY_THUMB_CLIP);
        QVERIFY(t.source().isEmpty());
        pictures.clear();

        t.request(13.5);
        QVERIFY(pictures.wait(10000));
        QCOMPARE(secondShown(t.image()), 12); // the keyframe before 13.5 s
        QCOMPARE(t.image().width(), Thumbnailer::MaxSize); // 640x360 fits as 360x203
        QVERIFY(qAbs(t.image().height() - 203) <= 1);
        QCOMPARE(t.source().scheme(), QString("image"));

        QVERIFY(pictures.wait(5000)); // the pointer rested: the exact frame follows
        QCOMPARE(secondShown(t.image()), 13);
        QVERIFY(!pictures.wait(Thumbnailer::SettleMs * 3)); // and nothing more

        t.request(13.5); // the same time again: already exact
        QVERIFY(!pictures.wait(Thumbnailer::SettleMs * 3));
    }

    void newestRequestWins()
    {
        Thumbnailer t(nullptr);
        t.setFile(ECHOPLAY_THUMB_CLIP);
        t.request(5.2);
        t.request(21.4);
        t.request(33.9);
        QTRY_COMPARE_WITH_TIMEOUT(secondShown(t.image()), 33, 10000);
        QSignalSpy pictures(&t, &Thumbnailer::imageChanged);
        QVERIFY(!pictures.wait(Thumbnailer::SettleMs * 3)); // the older requests were not served after it
        QCOMPARE(secondShown(t.image()), 33);

        // A move while the exact seek may still run: served by keyframe, then exact again.
        t.request(7.5);
        QTRY_COMPARE_WITH_TIMEOUT(secondShown(t.image()), 7, 10000);
    }

    void reopensAfterIdle()
    {
        Thumbnailer t(nullptr);
        t.setFile(ECHOPLAY_THUMB_CLIP);
        t.request(9.5);
        QTRY_COMPARE_WITH_TIMEOUT(secondShown(t.image()), 9, 10000);
        QSignalSpy pictures(&t, &Thumbnailer::imageChanged);
        QVERIFY(!pictures.wait(Thumbnailer::IdleMs + 500)); // closed the file: the picture stays
        QCOMPARE(secondShown(t.image()), 9);
        t.request(17.2);
        QTRY_COMPARE_WITH_TIMEOUT(secondShown(t.image()), 17, 10000);
    }

    void fileChangeClearsAndReopens()
    {
        Thumbnailer t(nullptr);
        QSignalSpy fileChanged(&t, &Thumbnailer::fileChanged);
        t.setFile(ECHOPLAY_THUMB_CLIP);
        t.request(30);
        QTRY_COMPARE_WITH_TIMEOUT(secondShown(t.image()), 30, 10000);

        t.setFile(QString());
        QCOMPARE(fileChanged.count(), 2);
        QVERIFY(t.image().isNull());
        QVERIFY(t.source().isEmpty());
        QSignalSpy pictures(&t, &Thumbnailer::imageChanged);
        t.request(10); // no file: nothing
        QVERIFY(!pictures.wait(500));

        t.setFile(QDir::temp().filePath("echoplay-no-such-clip.mp4")); // cannot be opened: no picture
        t.request(10);
        QVERIFY(!pictures.wait(1000));
        QCOMPARE(pictures.count(), 1); // only setFile's clear

        t.setFile(ECHOPLAY_THUMB_CLIP); // the hidden player opens the next file on request
        t.request(24.2);
        QTRY_COMPARE_WITH_TIMEOUT(secondShown(t.image()), 24, 10000);
    }

    // HDR10 red comes out red, not dull brown; the next, SDR file gets the plain filter again.
    void hdrIsToneMapped()
    {
        Thumbnailer t(nullptr);
        t.setFile(ECHOPLAY_HDR_CLIP);
        t.request(2);
        QTRY_VERIFY_WITH_TIMEOUT(!t.image().isNull(), 10000);
        const QColor red = t.image().pixelColor(t.image().width() / 2, t.image().height() / 2);
        QVERIFY2(red.red() > 180 && red.green() < 40 && red.blue() < 40, qPrintable(red.name()));
        t.setFile(ECHOPLAY_THUMB_CLIP);
        t.request(30);
        QTRY_COMPARE_WITH_TIMEOUT(secondShown(t.image()), 30, 10000); // its brightness tells the second: no tone mapping
    }
};

QTEST_GUILESS_MAIN(TestThumbnailer)
#include "tst_thumbnailer.moc"
