#include <QtTest>
#include <clocale>
#include "SilenceCheck.h"

// The hidden libmpv player on a generated 20 s clip with four audio tracks (tests/CMakeLists.txt):
// 1 a tone, 2 digital silence, 3 a tone at about -58 dBFS (quiet, not silent), 4 silent until 15 s, then a tone.

class TestSilenceCheck : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        std::setlocale(LC_NUMERIC, "C"); // libmpv requires it
        QVERIFY2(QFileInfo::exists(ECHOPLAY_SILENCE_CLIP), "the clip is made by ctest's silencecheck-clip step");
    }

    void findsTheSilentTrack()
    {
        SilenceCheck check(nullptr);
        QSignalSpy changed(&check, &SilenceCheck::changed);
        check.setFile(ECHOPLAY_SILENCE_CLIP);
        QVERIFY(!check.done());
        QVERIFY(!changed.wait(SilenceCheck::StartDelayMs - 500)); // waits before it starts
        QTRY_VERIFY_WITH_TIMEOUT(check.done(), 15000);
        // Track 4 is silent for 15 s: only reading on to the end tells it apart from track 2.
        QCOMPARE(check.silent(), QVariantList{2});
    }

    void fileChangeStartsOver()
    {
        SilenceCheck check(nullptr);
        check.setFile(ECHOPLAY_SILENCE_CLIP);
        QTRY_VERIFY_WITH_TIMEOUT(check.done(), 15000);

        check.setFile(QString());
        QVERIFY(!check.done());
        QVERIFY(check.silent().isEmpty());

        check.setFile(QDir::temp().filePath("echoplay-no-such-clip.mp4")); // cannot be read: nothing reported
        QTRY_VERIFY_WITH_TIMEOUT(check.done(), 10000);
        QVERIFY(check.silent().isEmpty());

        check.setFile(ECHOPLAY_SILENCE_CLIP); // the same hidden player reads the next file
        QTRY_VERIFY_WITH_TIMEOUT(check.done(), 15000);
        QCOMPARE(check.silent(), QVariantList{2});
    }
};

QTEST_GUILESS_MAIN(TestSilenceCheck)
#include "tst_silencecheck.moc"
