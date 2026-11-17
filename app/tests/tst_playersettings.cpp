#include <QtTest>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include "AppState.h"
#include "PlayerSettings.h"

class TestPlayerSettings : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QString("state-%1.json").arg(m_n++));
        m_state = std::make_unique<AppState>(m_path);
        m_settings = std::make_unique<PlayerSettings>(m_state.get());
    }

    void defaultsAreV1s()
    {
        QCOMPARE(m_settings->endMode(), QString("stop"));
        QCOMPARE(m_settings->speedFactor(), 2.0);
        QCOMPARE(m_settings->speedStep(), 0.1);
        QCOMPARE(m_settings->screenshotFolder(), QString("app")); // EchoPlay's own folder, not v1's desktop
        const QVariantMap o = m_settings->mpvOptions("C:/Videos/a.mp4");
        QCOMPARE(o.value("keep-open").toString(), QString("always")); // stays on the last frame, no next file
        QCOMPARE(o.value("loop-file").toString(), QString("no"));
        QCOMPARE(o.value("screenshot-dir").toString(), appShots());
    }

    void endModes()
    {
        QSignalSpy changed(m_settings.get(), &PlayerSettings::changed);
        m_settings->setEndMode("loop");
        QVariantMap o = m_settings->mpvOptions({});
        QCOMPARE(o.value("loop-file").toString(), QString("inf"));
        QCOMPARE(o.value("keep-open").toString(), QString("no"));
        m_settings->setEndMode("next");
        o = m_settings->mpvOptions({});
        QCOMPARE(o.value("loop-file").toString(), QString("no"));
        QCOMPARE(o.value("keep-open").toString(), QString("no"));
        m_settings->setEndMode("pause"); // not a mode: ignored
        QCOMPARE(m_settings->endMode(), QString("next"));
        QCOMPARE(changed.count(), 2);
    }

    void speedKeys()
    {
        QCOMPARE(m_settings->toggledSpeed(1.0), 2.0);
        QCOMPARE(m_settings->toggledSpeed(2.0), 1.0);
        QCOMPARE(m_settings->toggledSpeed(1.1), 1.0); // any faster speed goes back to normal
        QCOMPARE(m_settings->toggledSpeed(0.5), 2.0);
        m_settings->setSpeedFactor(9);
        QCOMPARE(m_settings->speedFactor(), PlayerSettings::SpeedFactorMax);
        m_settings->setSpeedFactor(1);
        QCOMPARE(m_settings->speedFactor(), PlayerSettings::SpeedFactorMin);
        m_settings->setSpeedStep(0.5);
        QCOMPARE(m_settings->speedStep(), 0.5);
        m_settings->setSpeedStep(0);
        m_settings->setSpeedStep(2);
        QCOMPARE(m_settings->speedStep(), 0.5);
    }

    void screenshotFolders()
    {
        m_settings->setScreenshotFolder("video_folder");
        QCOMPARE(m_settings->mpvOptions(QDir::rootPath() + "Videos/Clips/a.mp4").value("screenshot-dir").toString(),
                 QDir::rootPath() + "Videos/Clips"); // C:/ on Windows, / elsewhere
        // Where the chosen place does not apply, EchoPlay's folder gets the shot
        QCOMPARE(m_settings->mpvOptions("https://example.com/a.mp4").value("screenshot-dir").toString(), appShots());
        m_settings->setScreenshotFolder("custom"); // no folder picked yet
        QCOMPARE(m_settings->mpvOptions({}).value("screenshot-dir").toString(), appShots());
        m_settings->setScreenshotCustomPath("D:/Shots");
        QCOMPARE(m_settings->mpvOptions({}).value("screenshot-dir").toString(), QString("D:/Shots"));
        m_settings->setScreenshotFolder("desktop");
        QCOMPARE(m_settings->mpvOptions({}).value("screenshot-dir").toString(),
                 QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
        m_settings->setScreenshotFolder("app");
        QCOMPARE(m_settings->screenshotPath(), QDir::toNativeSeparators(appShots()));
        m_settings->setScreenshotFolder("pictures");
        QCOMPARE(m_settings->mpvOptions({}).value("screenshot-dir").toString(),
                 QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
        m_settings->setScreenshotFolder("cloud"); // not a place: ignored
        QCOMPARE(m_settings->screenshotFolder(), QString("pictures"));
    }

    void seekStepVolumeMaxAndChoices()
    {
        QCOMPARE(m_settings->seekStep(), 10); // v1's arrow step
        m_settings->setSeekStep(5);
        m_settings->setSeekStep(7); // not on the list: ignored
        QCOMPARE(m_settings->seekStep(), 5);

        QCOMPARE(m_settings->volumeMax(), 300);
        QCOMPARE(m_settings->mpvOptions({}).value("volume-max").toString(), QString("300"));
        m_settings->setVolumeMax(50);
        QCOMPARE(m_settings->volumeMax(), 100);
        m_settings->setVolumeMax(900);
        QCOMPARE(m_settings->volumeMax(), 300);
        m_settings->setVolumeMax(200);
        QCOMPARE(m_settings->mpvOptions({}).value("volume-max").toString(), QString("200"));

        QCOMPARE(m_settings->tracksOnOpen(), QString("last"));
        m_settings->setTracksOnOpen("all");
        m_settings->setTracksOnOpen("some");
        QCOMPARE(m_settings->tracksOnOpen(), QString("all"));

        QCOMPARE(m_settings->resumeMode(), QString("ask"));
        m_settings->setResumeMode("never");
        m_settings->setResumeMode("maybe");
        QCOMPARE(m_settings->resumeMode(), QString("never"));
    }

    void loudnessBoostAndAudioDevice()
    {
        const QString medium = "dynaudnorm=p=0.9:m=25:g=15"; // v1's
        QCOMPARE(m_settings->mpvOptions({}).value("af").toString(), medium);
        m_settings->setLoudnessStrength("high");
        const QString high = m_settings->mpvOptions({}).value("af").toString();
        m_settings->setLoudnessStrength("low");
        const QString low = m_settings->mpvOptions({}).value("af").toString();
        QVERIFY(high.startsWith("dynaudnorm") && low.startsWith("dynaudnorm"));
        QVERIFY(high != medium && low != medium && high != low);
        m_settings->setLoudnessStrength("max"); // not a strength: ignored
        QCOMPARE(m_settings->loudnessStrength(), QString("low"));
        m_settings->setLoudnessBoost(false);
        QCOMPARE(m_settings->mpvOptions({}).value("af").toString(), QString());

        QCOMPARE(m_settings->mpvOptions({}).value("audio-device").toString(), QString("auto"));
        m_settings->setAudioDevice("wasapi/{1234}");
        QCOMPARE(m_settings->mpvOptions({}).value("audio-device").toString(), QString("wasapi/{1234}"));
        m_settings->setAudioDevice(QString());
        QCOMPARE(m_settings->audioDevice(), QString("wasapi/{1234}"));
    }

    void pictureAdjustments()
    {
        const QStringList names{"brightness", "contrast", "saturation", "gamma"};
        QSignalSpy changed(m_settings.get(), &PlayerSettings::changed);
        QCOMPARE(m_settings->picture().size(), 4);
        const QVariantMap defaults = m_settings->mpvOptions({});
        for (const QString &name : names) {
            QCOMPARE(m_settings->picture().value(name).toInt(), 0);
            QCOMPARE(defaults.value(name).toString(), QString("0"));
        }

        m_settings->setPicture("brightness", 25);
        QCOMPARE(m_settings->picture().value("brightness").toInt(), 25);
        QCOMPARE(m_settings->mpvOptions({}).value("brightness").toString(), QString("25"));
        QCOMPARE(changed.count(), 1);
        m_settings->setPicture("brightness", 25); // the same value: no change
        QCOMPARE(changed.count(), 1);

        m_settings->setPicture("contrast", 500);
        m_settings->setPicture("saturation", -500);
        QCOMPARE(m_settings->picture().value("contrast").toInt(), 100);
        QCOMPARE(m_settings->picture().value("saturation").toInt(), -100);
        QCOMPARE(m_settings->mpvOptions({}).value("saturation").toString(), QString("-100"));
        QCOMPARE(changed.count(), 3);

        m_settings->setPicture("hue", 10); // not an adjustment: ignored
        QCOMPARE(m_settings->picture().size(), 4);
        QVERIFY(!m_settings->mpvOptions({}).contains("hue"));
        QCOMPARE(changed.count(), 3);

        m_settings->resetPicture();
        for (const QString &name : names) {
            QCOMPARE(m_settings->picture().value(name).toInt(), 0);
            QCOMPARE(m_settings->mpvOptions({}).value(name).toString(), QString("0"));
        }
        QCOMPARE(changed.count(), 4); // one signal for all
        m_settings->resetPicture();   // nothing to reset: no signal
        QCOMPARE(changed.count(), 4);
    }

    void subtitleSizeAndPosition()
    {
        QSignalSpy changed(m_settings.get(), &PlayerSettings::changed);
        QCOMPARE(m_settings->subtitleScale(), 1.0);
        QCOMPARE(m_settings->subtitlePosition(), 100);
        const QVariantMap defaults = m_settings->mpvOptions({});
        QCOMPARE(defaults.value("sub-scale").toString(), QString("1"));
        QCOMPARE(defaults.value("sub-pos").toString(), QString("100"));

        m_settings->setSubtitleScale(1.5);
        m_settings->setSubtitlePosition(90);
        QCOMPARE(m_settings->mpvOptions({}).value("sub-scale").toString(), QString("1.5"));
        QCOMPARE(m_settings->mpvOptions({}).value("sub-pos").toString(), QString("90"));
        QCOMPARE(changed.count(), 2);
        m_settings->setSubtitleScale(1.5); // the same values: no change
        m_settings->setSubtitlePosition(90);
        QCOMPARE(changed.count(), 2);

        m_settings->setSubtitleScale(9);
        QCOMPARE(m_settings->subtitleScale(), 3.0);
        m_settings->setSubtitleScale(0);
        QCOMPARE(m_settings->subtitleScale(), 0.5);
        m_settings->setSubtitlePosition(500);
        QCOMPARE(m_settings->subtitlePosition(), 150);
        m_settings->setSubtitlePosition(-5);
        QCOMPARE(m_settings->subtitlePosition(), 0);
        QCOMPARE(m_settings->mpvOptions({}).value("sub-pos").toString(), QString("0"));

        // Twelve +0.1 steps from 1.0 must land on 2.2, not 2.2000000000000006
        m_settings->setSubtitleScale(1.0);
        for (int i = 0; i < 12; ++i) m_settings->setSubtitleScale(m_settings->subtitleScale() + 0.1);
        QCOMPARE(m_settings->subtitleScale(), 2.2);
        QCOMPARE(m_settings->mpvOptions({}).value("sub-scale").toString(), QString("2.2"));
    }

    void shortcutsKeepOnlyWhatChanged()
    {
        QVERIFY(m_settings->shortcuts().isEmpty());
        m_settings->setShortcut("mute", "N");
        QCOMPARE(m_settings->shortcuts().value("mute").toString(), QString("N"));
        QSignalSpy changed(m_settings.get(), &PlayerSettings::changed);
        m_settings->setShortcut("mute", "N"); // the same key: no change
        QCOMPARE(changed.count(), 0);
        m_settings->resetShortcuts();
        QVERIFY(m_settings->shortcuts().isEmpty());
        QCOMPARE(changed.count(), 1);
    }

    void keyNames()
    {
        QCOMPARE(PlayerSettings::keyName(Qt::Key_K, Qt::NoModifier), QString("K"));
        QCOMPARE(PlayerSettings::keyName(Qt::Key_S, Qt::ShiftModifier), QString("Shift+S"));
        QCOMPARE(PlayerSettings::keyName(Qt::Key_1, Qt::ControlModifier), QString("Ctrl+1"));
        QCOMPARE(PlayerSettings::keyName(Qt::Key_Greater, Qt::ShiftModifier), QString(">")); // as v1's default reads
        QCOMPARE(PlayerSettings::keyName(Qt::Key_Space, Qt::NoModifier), QString("Space"));
        QCOMPARE(PlayerSettings::keyName(Qt::Key_Up, Qt::KeypadModifier), QString("Up"));
        QCOMPARE(PlayerSettings::keyName(Qt::Key_Control, Qt::ControlModifier), QString()); // the key comes next
    }

    void savedAndLoaded()
    {
        m_settings->setEndMode("loop");
        m_settings->setSpeedFactor(1.5);
        m_settings->setSpeedStep(0.05);
        m_settings->setScreenshotFolder("custom");
        m_settings->setScreenshotCustomPath("D:/Shots");
        m_settings->setSeekStep(30);
        m_settings->setVolumeMax(150);
        m_settings->setLoudnessBoost(false);
        m_settings->setLoudnessStrength("high");
        m_settings->setAudioDevice("wasapi/{1234}");
        m_settings->setTracksOnOpen("all");
        m_settings->setResumeMode("always");
        m_settings->setPicture("brightness", -10);
        m_settings->setPicture("gamma", 35);
        m_settings->setSubtitleScale(1.3);
        m_settings->setSubtitlePosition(85);
        m_settings->setFirstRunHintDone(true);
        m_settings->setClickPauses(false);
        m_settings->setShowRemaining(true);
        m_settings->setAlwaysOnTop(true);
        m_settings->setWindowPlacement({{"x", -40}, {"y", 20}, {"width", 900}, {"height", 500}, {"maximized", true}});
        m_settings->setShortcut("mute", "N");
        m_state->save();

        AppState again(m_path);
        PlayerSettings loaded(&again);
        QCOMPARE(loaded.endMode(), QString("loop"));
        QCOMPARE(loaded.speedFactor(), 1.5);
        QCOMPARE(loaded.speedStep(), 0.05);
        QCOMPARE(loaded.screenshotFolder(), QString("custom"));
        QCOMPARE(loaded.screenshotCustomPath(), QString("D:/Shots"));
        QCOMPARE(loaded.seekStep(), 30);
        QCOMPARE(loaded.volumeMax(), 150);
        QVERIFY(!loaded.loudnessBoost());
        QCOMPARE(loaded.loudnessStrength(), QString("high"));
        QCOMPARE(loaded.audioDevice(), QString("wasapi/{1234}"));
        QCOMPARE(loaded.tracksOnOpen(), QString("all"));
        QCOMPARE(loaded.resumeMode(), QString("always"));
        QCOMPARE(loaded.picture().value("brightness").toInt(), -10);
        QCOMPARE(loaded.picture().value("contrast").toInt(), 0);
        QCOMPARE(loaded.mpvOptions({}).value("gamma").toString(), QString("35"));
        QCOMPARE(loaded.subtitleScale(), 1.3);
        QCOMPARE(loaded.subtitlePosition(), 85);
        QVERIFY(loaded.firstRunHintDone());
        QVERIFY(!loaded.clickPauses());
        QVERIFY(loaded.showRemaining());
        QVERIFY(loaded.alwaysOnTop());
        QCOMPARE(loaded.windowPlacement().value("x").toInt(), -40); // left of the main screen: a second monitor
        QCOMPARE(loaded.windowPlacement().value("height").toInt(), 500);
        QVERIFY(loaded.windowPlacement().value("maximized").toBool());
        QCOMPARE(loaded.shortcuts().value("mute").toString(), QString("N"));
    }

    void damagedValuesKeepDefaults()
    {
        QFile f(m_path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"schema":1,"always_on_top":"x","window_placement":{"x":1,"y":1,"width":-5,"height":100},)"
                R"("playback":{"end":"explode","speed_factor":50,"speed_step":-1,"seek_step":7,"resume":1,"click_pauses":1,"show_remaining":"x"},)"
                R"("audio":{"volume_max":999,"loudness_boost":"yes","loudness_strength":"max","device":3,"tracks_on_open":"x"},)"
                R"("picture":{"brightness":500,"contrast":"x","saturation":1.5},)"
                R"("subtitles":{"scale":"big","position":999},)"
                R"("first_run_hint_done":"yes","shortcuts":{"mute":5},)"
                R"("screenshots":{"folder":7,"custom":["x"]}})");
        f.close();
        AppState damaged(m_path);
        PlayerSettings loaded(&damaged);
        QCOMPARE(loaded.endMode(), QString("stop"));
        QCOMPARE(loaded.speedFactor(), 2.0);
        QCOMPARE(loaded.speedStep(), 0.1);
        QCOMPARE(loaded.screenshotFolder(), QString("app"));
        QCOMPARE(loaded.screenshotCustomPath(), QString());
        QCOMPARE(loaded.seekStep(), 10);
        QCOMPARE(loaded.volumeMax(), 300);
        QVERIFY(loaded.loudnessBoost());
        QCOMPARE(loaded.loudnessStrength(), QString("medium"));
        QCOMPARE(loaded.audioDevice(), QString("auto"));
        QCOMPARE(loaded.tracksOnOpen(), QString("last"));
        QCOMPARE(loaded.resumeMode(), QString("ask"));
        QCOMPARE(loaded.picture().value("brightness").toInt(), 0);
        QCOMPARE(loaded.picture().value("contrast").toInt(), 0);
        QCOMPARE(loaded.picture().value("saturation").toInt(), 0);
        QCOMPARE(loaded.subtitleScale(), 1.0);
        QCOMPARE(loaded.subtitlePosition(), 100);
        QVERIFY(!loaded.firstRunHintDone());
        QVERIFY(loaded.clickPauses());
        QVERIFY(!loaded.showRemaining());
        QVERIFY(!loaded.alwaysOnTop());
        QVERIFY(loaded.windowPlacement().isEmpty());
        QVERIFY(loaded.shortcuts().isEmpty());
    }

private:
    QString appShots() const { return QFileInfo(m_path).absolutePath() + "/Screenshots"; } // next to state.json
    QTemporaryDir m_dir;
    int m_n = 0;
    QString m_path;
    std::unique_ptr<AppState> m_state;
    std::unique_ptr<PlayerSettings> m_settings;
};

QTEST_GUILESS_MAIN(TestPlayerSettings)
#include "tst_playersettings.moc"
