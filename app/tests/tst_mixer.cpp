#include <QtTest>
#include "Mixer.h"

namespace {

QVariantMap audio(int id, const QString &title = {}, const QString &lang = {}, const QVariantMap &metadata = {})
{
    QVariantMap t{{"type", "audio"}, {"id", id}, {"codec", "aac"}};
    if (!title.isEmpty()) t.insert("title", title);
    if (!lang.isEmpty()) t.insert("lang", lang);
    if (!metadata.isEmpty()) t.insert("metadata", metadata);
    return t;
}

// A typical ShadowPlay clip: one video track and n audio tracks.
QVariantList clip(int audioTracks)
{
    QVariantList list{QVariantMap{{"type", "video"}, {"id", 1}, {"codec", "hevc"}}};
    for (int id = 1; id <= audioTracks; ++id) list.append(audio(id));
    return list;
}

Mixer loaded(int audioTracks, const MixerPrefs &prefs = {})
{
    Mixer m;
    m.setTracks(clip(audioTracks));
    m.applyPrefs(prefs);
    return m;
}

const QString Mono = ",pan=stereo|c0=0.5*c0+0.5*c1|c1=0.5*c0+0.5*c1";

} // namespace

class TestMixer : public QObject
{
    Q_OBJECT

private slots:
    void keepsAudioTracksInOrder()
    {
        Mixer m;
        QVERIFY(!m.setTracks({QVariantMap{{"type", "video"}, {"id", 1}}}));
        QVERIFY(m.tracks().isEmpty());
        QVERIFY(m.setTracks(clip(3)));
        QCOMPARE(m.tracks().size(), 3);
        QCOMPARE(m.tracks()[0].id, 1);
        QCOMPARE(m.tracks()[2].id, 3);
        QCOMPARE(m.tracks()[0].codec, QString("aac"));
    }

    void names()
    {
        Mixer m;
        m.setTracks({audio(1, "System sounds", "eng"), audio(2, {}, {}, {{"title", "Meta title"}}),
                     audio(3, {}, {}, {{"name", "Meta name"}}), audio(4, {}, {}, {{"NAME", "Upper name"}}),
                     audio(5, {}, "tur"), audio(6, {}, "und"), audio(7)});
        const QStringList want{"System sounds", "Meta title", "Meta name", "Upper name", "TUR", "", ""};
        for (int i = 0; i < want.size(); ++i) QCOMPARE(m.tracks()[i].name, want[i]);
    }

    void output_data()
    {
        QTest::addColumn<QString>("defaultOn");
        QTest::addColumn<double>("gain2");
        QTest::addColumn<bool>("mono1");
        QTest::addColumn<QString>("lavfi");
        QTest::addColumn<QString>("aid");
        QTest::newRow("none") << "none" << 1.0 << false << "" << "no";
        QTest::newRow("one plain track: no graph") << "2" << 1.0 << false << "" << "2";
        QTest::newRow("one track with gain") << "2" << 1.5 << false << "[aid2]volume=1.50[ao]" << "no";
        QTest::newRow("one mono track") << "1" << 1.0 << true << "[aid1]volume=1.00" + Mono + "[ao]" << "no";
        QTest::newRow("two tracks")
            << "all" << 0.5 << false
            << "[aid1]volume=1.00[ev1];[aid2]volume=0.50[ev2];[ev1][ev2]amix=inputs=2:normalize=0[ao]" << "no";
        QTest::newRow("two tracks, first mono")
            << "1,2" << 1.0 << true
            << "[aid1]volume=1.00" + Mono + "[ev1];[aid2]volume=1.00[ev2];[ev1][ev2]amix=inputs=2:normalize=0[ao]"
            << "no";
    }
    void output()
    {
        QFETCH(QString, defaultOn);
        QFETCH(double, gain2);
        QFETCH(bool, mono1);
        MixerPrefs p;
        p.defaultOn = defaultOn;
        if (gain2 != 1.0) p.gains.insert(2, gain2);
        if (mono1) p.mono.insert(1);
        const Mixer::Output out = loaded(2, p).output();
        QTEST(out.lavfiComplex, "lavfi");
        QTEST(out.aid, "aid");
    }

    void threeTracksMixInTrackOrder()
    {
        MixerPrefs p;
        p.defaultOn = "3,1"; // order in prefs does not matter
        Mixer m = loaded(3, p);
        QCOMPARE(m.enabledTracks(), (QList<int>{1, 3}));
        QCOMPARE(m.output().lavfiComplex,
                 QString("[aid1]volume=1.00[ev1];[aid3]volume=1.00[ev3];[ev1][ev3]amix=inputs=2:normalize=0[ao]"));
        m.setEnabled(2, true);
        QVERIFY(m.output().lavfiComplex.endsWith("[ev1][ev2][ev3]amix=inputs=3:normalize=0[ao]"));
    }

    void defaults()
    {
        MixerPrefs p; // first run: track 1 only
        QCOMPARE(loaded(2, p).enabledTracks(), QList<int>{1});
        p.defaultOn = "all";
        QCOMPARE(loaded(3, p).enabledTracks(), (QList<int>{1, 2, 3}));
        p.defaultOn = "none";
        QVERIFY(loaded(2, p).enabledTracks().isEmpty());
        p.defaultOn = "5"; // saved on a clip with more tracks: never open a clip silent by accident
        QCOMPARE(loaded(2, p).enabledTracks(), QList<int>{1});
        p.defaultOn = "1,3";
        QCOMPARE(loaded(2, p).enabledTracks(), QList<int>{1});
        QCOMPARE(loaded(3, p).enabledTracks(), (QList<int>{1, 3}));
    }

    void applyPrefsResetsThePreviousFile()
    {
        Mixer m = loaded(2);
        m.setEnabled(2, true);
        m.setMono(1, true);
        m.setTracks(clip(2));
        m.applyPrefs({});
        QCOMPARE(m.enabledTracks(), QList<int>{1});
        QVERIFY(!m.isMono(1));
    }

    void gain()
    {
        MixerPrefs p;
        p.defaultOn = "1";
        Mixer m = loaded(2, p);
        QCOMPARE(m.gain(2), 1.0);
        m.setGain(2, m.gain(2) + 0.1);
        m.setGain(2, m.gain(2) + 0.1);
        m.setGain(2, m.gain(2) - 0.1);
        m.setGain(2, m.gain(2) - 0.1);
        QCOMPARE(m.gain(2), 1.0); // exactly: no floating drift saved as 1.0000000000000002
        QVERIFY(m.isEnabled(2));  // changing a track's gain turns it on, like v1
        m.setGain(2, 9.0);
        QCOMPARE(m.gain(2), Mixer::GainMax);
        m.setGain(2, -1.0);
        QCOMPARE(m.gain(2), 0.0);
        m.setGain(2, 1.234);
        QCOMPARE(m.gain(2), 1.23);
    }

    void monoEnablesTheTrack()
    {
        Mixer m = loaded(2);
        m.setMono(2, true);
        QVERIFY(m.isEnabled(2));
        QVERIFY(m.isMono(2));
        m.setMono(2, false);
        QVERIFY(!m.isMono(2));
    }

    void prefsRoundTrip()
    {
        Mixer m = loaded(3);
        m.setAllEnabled(true);
        QCOMPARE(m.prefs().defaultOn, QString("all"));
        m.setAllEnabled(false);
        QCOMPARE(m.prefs().defaultOn, QString("none"));
        m.setGain(1, 1.5);
        m.setGain(2, 1.0); // back at 100 %: not saved
        m.setEnabled(2, false);
        m.setMono(3, true);
        const MixerPrefs p = m.prefs();
        QCOMPARE(p.defaultOn, QString("1,3"));
        QCOMPARE(p.gains, (QMap<int, double>{{1, 1.5}}));
        QCOMPARE(p.mono, QSet<int>{3});
        Mixer again = loaded(3, p);
        QCOMPARE(again.prefs(), p);
        QCOMPARE(again.output().lavfiComplex, m.output().lavfiComplex);
    }
};

QTEST_GUILESS_MAIN(TestMixer)
#include "tst_mixer.moc"
