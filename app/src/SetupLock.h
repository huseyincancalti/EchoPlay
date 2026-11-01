#pragma once

#include <QString>
#ifdef Q_OS_WIN
#include <string>
#endif

// Keeps a new EchoPlay away from the files the setup is replacing. A running EchoPlay holds its exe and DLLs: the
// silent setup would close its window, or fail on the file. While the setup installs (CurStepChanged in
// installer/windows/v2/EchoPlay.iss) a new launch leaves at once and comes back when it is done. A wizard left open
// holds nothing back: that setup asks to close EchoPlay when it gets there, as any setup does.
namespace SetupLock {

bool isHeld(); // a setup of EchoPlay is installing, or about to

// Starts this same command line again when the setup is done, from a hidden process that holds none of EchoPlay's
// files. False when it could not: the caller starts anyway.
bool comeBackLater();

// Takes the lock now, for an EchoPlay that will start the setup when it has closed: a clip opened meanwhile comes back
// after the setup. The process keeps it until startSetup or its own end.
void hold();

// Starts the setup silently, holding the lock from before its first instruction: Inno Setup takes its own mutex only
// after unpacking itself, and a clip opened right after closing EchoPlay must not slip through. reopen starts the new
// EchoPlay when it is done. False when it did not start, or when Windows is shutting down and would cut it off.
bool startSetup(const QString &path, bool reopen);

#ifdef Q_OS_WIN
// One command line argument in quotes, as CommandLineToArgvW gives it back (tested with that round trip).
std::wstring quoteArgument(const std::wstring &arg);
#endif

} // namespace SetupLock
