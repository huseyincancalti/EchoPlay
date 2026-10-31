#include <QtTest>
#include "AppState.h"
#include "PictureQuality.h"

class TestPictureQuality : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_state = std::make_unique<AppState>(m_dir.filePath(QString("state-%1.json").arg(m_n++)));
        m_quality = std::make_unique<PictureQuality>(m_state.get());
    }

    void startsInAutoAtNormal()
    {
        QCOMPARE(m_quality->pref(), QString("auto"));
        QCOMPARE(m_quality->active(), QString("normal"));
        QCOMPARE(m_quality->activeOptions().value("scale").toString(), QString("lanczos"));
    }

    // v1's tests (tests/run.lua, quality.lua section), same sequence.
    void stepsDownAfterThreeBadTicksAndStaysThere()
    {
        QSignalSpy applied(m_quality.get(), &PictureQuality::applyRequested);
        m_quality->tick(0); // the first tick only sets the baseline
        for (int i = 1; i <= 3; ++i) m_quality->tick(i * 10);
        QCOMPARE(m_quality->active(), QString("low"));
        QCOMPARE(applied.count(), 1);
        QCOMPARE(applied[0][0].toMap().value("scale").toString(), QString("bilinear"));

        double count = 30;
        for (int i = 0; i < 10; ++i) m_quality->tick(++count); // clean ticks, but Normal proved too heavy
        QCOMPARE(m_quality->active(), QString("low"));

        m_quality->fileStarted(); // a new file may try Normal again
        m_quality->tick(0);
        for (int i = 1; i <= 5; ++i) m_quality->tick(i);
        QCOMPARE(m_quality->active(), QString("normal"));
    }

    void ultraNeedsALongCleanStretch()
    {
        m_quality->tick(0);
        for (int i = 0; i < 14; ++i) m_quality->tick(0);
        QCOMPARE(m_quality->active(), QString("normal"));
        m_quality->tick(0);
        QCOMPARE(m_quality->active(), QString("ultra"));
        QCOMPARE(m_quality->activeOptions().value("scale").toString(), QString("ewa_lanczossharp"));
    }

    void aCounterResetIsNotABadTick()
    {
        m_quality->tick(500);
        for (int i = 0; i < 5; ++i) m_quality->tick(0); // mpv reset frame-drop-count (mixer switch)
        QCOMPARE(m_quality->active(), QString("normal"));
    }

    void forcedTierAppliesAtOnceAndIsSaved()
    {
        QSignalSpy applied(m_quality.get(), &PictureQuality::applyRequested);
        m_quality->setPref("ultra");
        QCOMPARE(m_quality->active(), QString("ultra"));
        QCOMPARE(applied.count(), 1);
        for (int i = 0; i < 10; ++i) m_quality->tick(i * 100); // forced: drops never change it
        QCOMPARE(m_quality->active(), QString("ultra"));
        QCOMPARE(m_state->quality(), QString("ultra"));
        m_quality->setPref("best"); // not a tier: ignored
        QCOMPARE(m_quality->pref(), QString("ultra"));
        QCOMPARE(PictureQuality(m_state.get()).active(), QString("ultra")); // next start begins there
    }

    void everyTierSetsTheSameKeys()
    {
        const QStringList keys = PictureQuality::options("normal").keys();
        QCOMPARE(PictureQuality::options("ultra").keys(), keys);
        QCOMPARE(PictureQuality::options("low").keys(), keys);
    }

private:
    QTemporaryDir m_dir;
    int m_n = 0;
    std::unique_ptr<AppState> m_state;
    std::unique_ptr<PictureQuality> m_quality;
};

QTEST_GUILESS_MAIN(TestPictureQuality)
#include "tst_picturequality.moc"
