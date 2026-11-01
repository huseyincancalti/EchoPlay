#include <QtTest>
#include "SetupLock.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <vector>
#endif

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
        // Inno Setup's SetupMutex, and the one startSetup holds for it before it has started.
        for (const wchar_t *name : {L"EchoPlaySetup", L"EchoPlaySetupStarting"}) {
            QVERIFY(!SetupLock::isHeld());
            HANDLE lock = CreateMutexW(nullptr, FALSE, name);
            QVERIFY(lock);
            QVERIFY(SetupLock::isHeld());
            CloseHandle(lock);
            QVERIFY(!SetupLock::isHeld());
        }
#endif
    }
};

QTEST_GUILESS_MAIN(TestSetupLock)
#include "tst_setuplock.moc"
