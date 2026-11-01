#pragma once

#include <QString>

// Keeps a new EchoPlay away from the files the setup is replacing. A running EchoPlay holds its exe and DLLs: the
// silent setup would close its window, or fail on the file. While the setup runs (SetupMutex in
// installer/windows/v2/EchoPlay.iss) a new launch leaves at once and comes back when it is done.
namespace SetupLock {

bool isHeld(); // a setup of EchoPlay is running

// Starts this same command line again when the setup is done, from a hidden process that holds none of EchoPlay's
// files. False when it could not: the caller starts anyway.
bool comeBackLater();

// Starts the setup silently, holding the lock from before its first instruction: Inno Setup takes its own mutex only
// after unpacking itself, and a clip opened right after closing EchoPlay must not slip through. reopen starts the new
// EchoPlay when it is done. False when it did not start, or when Windows is shutting down and would cut it off.
bool startSetup(const QString &path, bool reopen);

} // namespace SetupLock
