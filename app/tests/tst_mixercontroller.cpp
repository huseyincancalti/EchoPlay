#include <QtTest>
#include "AppState.h"
#include "MixerController.h"

namespace {

QVariantList clip(int audioTracks)
{
    QVariantList list{QVariantMap{{"type", "video"}, {"id", 1}}};
    for (int id = 1; id <= audioTracks; ++id)
        list.append(QVariantMap{{"type", "audio"}, {"id", id}, {"title", id == 1 ? "System sounds" : "Microphone"}});
    return list;
}

using Outputs = QList<QPair<QString, QString>>;

Outputs outputs(const QSignalSpy &spy)
{
    Outputs out;
    for (const QList<QVariant> &args : spy) out.append({args[0].toString(), args[1].toString()});
    return out;
}

const QString Mix = "[aid1]volume=1.00[ev1];[aid2]volume=1.00[ev2];[ev1][ev2]amix=inputs=2:normalize=0[ao]";

} // namespace

class TestMixerController : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_state = std::make_unique<AppState>(m_dir.filePath(QString("state-%1.json").arg(m_n++)));
        m_mixer = std::make_unique<MixerController>(m_state.get());
    }

    void appliesSavedPrefsOnceAudioTracksExist()
    {
        MixerPrefs prefs;
        prefs.defaultOn = "all";
        m_state->setMixer(prefs);
        QSignalSpy spy(m_mixer.get(), &MixerController::outputChanged);
        m_mixer->setTrackList({QVariantMap{{"type", "video"}, {"id", 1}}}); // audio not registered yet
        QCOMPARE(spy.count(), 0);
        QVERIFY(m_mixer->tracks().isEmpty());
        m_mixer->setTrackList(clip(2));
        QCOMPARE(outputs(spy), (Outputs{{Mix, "no"}}));
        QCOMPARE(m_mixer->tracks().size(), 2);
        QCOMPARE(m_mixer->tracks()[1].toMap().value("name").toString(), QString("Microphone"));
    }

    void tracksOnOpenAllOverridesTheLastClip()
    {
        MixerPrefs prefs;
        prefs.defaultOn = "2";
        m_state->setMixer(prefs);
        m_state->setTracksOnOpen("all");
        QSignalSpy spy(m_mixer.get(), &MixerController::outputChanged);
        m_mixer->setTrackList(clip(2));
        QCOMPARE(outputs(spy), (Outputs{{Mix, "no"}}));
        QCOMPARE(m_state->mixer().defaultOn, QString("2")); // nothing the user changed yet: nothing is saved
    }

    void laterTrackListsDoNotUndoTheUsersChoice()
    {
        m_mixer->setTrackList(clip(2)); // first run: track 1 only
        m_mixer->toggle(2);
        QSignalSpy spy(m_mixer.get(), &MixerController::outputChanged);
        m_mixer->setTrackList(clip(2));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m_mixer->tracks()[1].toMap().value("enabled").toBool(), true);
    }

    void nextFileDropsTheGraphThenAppliesAgain()
    {
        m_mixer->setTrackList(clip(2));
        m_mixer->toggle(2);
        QSignalSpy spy(m_mixer.get(), &MixerController::outputChanged);
        m_mixer->fileStarted();
        QVERIFY(m_mixer->tracks().isEmpty());
        m_mixer->setTrackList(clip(2));
        // Empty aid on start-file: only the old graph goes; the saved "1,2" comes back for the new file.
        QCOMPARE(outputs(spy), (Outputs{{"", ""}, {Mix, "no"}}));
    }

    void userChangesAreSavedAndApplied()
    {
        QSignalSpy spy(m_mixer.get(), &MixerController::outputChanged);
        m_mixer->setTrackList(clip(2));
        m_mixer->setGain(2, 1.5);
        m_mixer->setMono(1, true);
        m_mixer->setAllEnabled(false);
        QCOMPARE(m_state->mixer().defaultOn, QString("none"));
        QCOMPARE(m_state->mixer().gains, (QMap<int, double>{{2, 1.5}}));
        QCOMPARE(m_state->mixer().mono, QSet<int>{1});
        QCOMPARE(outputs(spy).first(), (QPair<QString, QString>{"", "1"}));
        QCOMPARE(outputs(spy).last(), (QPair<QString, QString>{"", "no"}));
    }

    void sameResultIsNotAppliedTwice()
    {
        m_mixer->setTrackList(clip(2));
        QSignalSpy spy(m_mixer.get(), &MixerController::outputChanged);
        m_mixer->setGain(1, 1.0); // already 100 %, already on
        QCOMPARE(spy.count(), 0);
    }

    void actionsBeforeTracksAreIgnored()
    {
        QSignalSpy spy(m_mixer.get(), &MixerController::outputChanged);
        m_mixer->toggle(1);
        m_mixer->setGain(1, 2.0);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m_state->mixer(), MixerPrefs{});
    }

private:
    QTemporaryDir m_dir;
    int m_n = 0;
    std::unique_ptr<AppState> m_state;
    std::unique_ptr<MixerController> m_mixer;
};

QTEST_GUILESS_MAIN(TestMixerController)
#include "tst_mixercontroller.moc"
