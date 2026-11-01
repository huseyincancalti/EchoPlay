#include <QtTest>
#include "SetupLock.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <vector>
#endif

// setupInheritsTheLockAndNothingElse starts this same program as the setup: it stays while this file exists.
constexpr char StayWhileFile[] = "ECHOPLAY_TEST_STAY_WHILE_FILE";

class TestSetupLock : public QObject
{
    Q_OBJECT

private slots:
    // What the relaunch passes on must be what the player was started with, in the reader Windows itself uses.
    void quotedArgumentsComeBackAsTheyWere()
    {
#ifdef Q_OS_WIN
        const std::vector<std::wstring> args = {L"plain",        L"",     L"C:\\x y\\",      L"a\"b",
                                                L"C:\\a b\\\"c", L"\\\\server\\share\\", L"d & e %f% (g) 'h' $i",
                                                L"https://x.y/a?b=c&d", L"\u00fc \u011f \u0131"};
        std::wstring line = L"prog.exe";
        for (const std::wstring &arg : args) line += L" " + SetupLock::quoteArgument(arg);
        int count = 0;
        wchar_t **parsed = CommandLineToArgvW(line.c_str(), &count);
        QVERIFY(parsed);
        QCOMPARE(count, int(args.size()) + 1);
        for (size_t i = 0; i < args.size(); ++i)
            QCOMPARE(QString::fromWCharArray(parsed[i + 1]), QString::fromStdWString(args[i]));
        LocalFree(parsed);
#endif
    }

    void heldOnlyWhileAMutexExists()
    {
#ifdef Q_OS_WIN
        if (SetupLock::isHeld()) QSKIP("a real setup of EchoPlay is installing on this machine");
        // The setup's while it installs, and the one hold takes before it has started.
        for (const wchar_t *name : {L"EchoPlayInstalling", L"EchoPlaySetupStarting"}) {
            QVERIFY(!SetupLock::isHeld());
            HANDLE lock = CreateMutexW(nullptr, FALSE, name);
            QVERIFY(lock);
            QVERIFY(SetupLock::isHeld());
            CloseHandle(lock);
            QVERIFY(!SetupLock::isHeld());
        }
#endif
    }

    // Only the lock goes along: anything else of EchoPlay's that the setup kept open would outlive EchoPlay.
    void setupInheritsTheLockAndNothingElse()
    {
#ifdef Q_OS_WIN
        if (SetupLock::isHeld()) QSKIP("a real setup of EchoPlay is installing on this machine");
        SECURITY_ATTRIBUTES inheritable{sizeof inheritable, nullptr, TRUE};
        const wchar_t *otherName = L"EchoPlayTestInheritable";
        HANDLE other = CreateEventW(&inheritable, FALSE, FALSE, otherName);
        QVERIFY(other);
        QTemporaryDir dir;
        const QString stayWhile = dir.filePath("running");
        QFile file(stayWhile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        qputenv(StayWhileFile, stayWhile.toLocal8Bit());

        SetupLock::hold(); // main.cpp, before it lets go of the pipe
        QVERIFY(SetupLock::isHeld());
        QVERIFY(SetupLock::startSetup(QCoreApplication::applicationFilePath(), false));
        qunsetenv(StayWhileFile);

        const bool lockGoesOn = SetupLock::isHeld(); // this process let go of it
        CloseHandle(other);
        HANDLE stillThere = OpenEventW(SYNCHRONIZE, FALSE, otherName);
        if (stillThere) CloseHandle(stillThere);
        QVERIFY(QFile::remove(stayWhile));
        QVERIFY(lockGoesOn);
        QVERIFY2(!stillThere, "the setup inherited more than the lock");
        QTRY_VERIFY(!SetupLock::isHeld()); // and it ends with the setup
#endif
    }
};

int main(int argc, char *argv[])
{
    const QString stayWhile = qEnvironmentVariable(StayWhileFile);
    if (!stayWhile.isEmpty()) {
        for (int i = 0; i < 1000 && QFile::exists(stayWhile); ++i) QThread::msleep(10);
        return 0;
    }
    QCoreApplication app(argc, argv);
    TestSetupLock test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_setuplock.moc"
