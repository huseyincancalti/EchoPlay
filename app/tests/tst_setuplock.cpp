#include <QtTest>
#include "SetupLock.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class TestSetupLock : public QObject
{
    Q_OBJECT

private slots:
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
