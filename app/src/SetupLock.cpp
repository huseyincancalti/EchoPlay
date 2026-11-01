#include "SetupLock.h"

#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <string>

namespace {

constexpr wchar_t SetupMutex[] = L"EchoPlaySetup"; // SetupMutex in installer/windows/v2/EchoPlay.iss
// The one startSetup holds. It cannot be the same one: Inno Setup quits without a word when its mutex already exists.
constexpr wchar_t StartMutex[] = L"EchoPlaySetupStarting";

// This process's arguments for its next start, past the lock whatever happens (a setup that hangs).
std::wstring ownArguments()
{
    std::wstring arguments = L"--after-setup";
    int count = 0;
    if (wchar_t **args = CommandLineToArgvW(GetCommandLineW(), &count)) {
        for (int i = 1; i < count; ++i) arguments += L" " + SetupLock::quoteArgument(args[i]);
        LocalFree(args);
    }
    return arguments;
}

std::wstring systemFile(const wchar_t *name)
{
    wchar_t dir[MAX_PATH];
    GetSystemDirectoryW(dir, MAX_PATH);
    return std::wstring(dir) + L"\\" + name; // full path: Explorer starts EchoPlay in the clip's folder
}

bool start(const std::wstring &program, std::wstring command, BOOL inheritHandles, DWORD flags)
{
    STARTUPINFOW info{};
    info.cb = sizeof info;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(program.c_str(), command.data(), nullptr, nullptr, inheritHandles, flags, nullptr, nullptr,
                        &info, &process))
        return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

} // namespace

// The rules CommandLineToArgvW reads back: backslashes count only before a quote (they double, and a quote gets one
// more), so a trailing \ or a " inside survives.
std::wstring SetupLock::quoteArgument(const std::wstring &arg)
{
    std::wstring out = L"\"";
    for (auto it = arg.begin();; ++it) {
        size_t slashes = 0;
        while (it != arg.end() && *it == L'\\') { ++it; ++slashes; }
        if (it == arg.end()) {
            out.append(slashes * 2, L'\\'); // before the closing quote
            break;
        }
        if (*it == L'"') out.append(slashes * 2 + 1, L'\\');
        else out.append(slashes, L'\\');
        out.push_back(*it);
    }
    out.push_back(L'"');
    return out;
}

bool SetupLock::isHeld()
{
    for (const wchar_t *name : {SetupMutex, StartMutex}) {
        if (HANDLE lock = OpenMutexW(SYNCHRONIZE, FALSE, name)) {
            CloseHandle(lock);
            return true;
        }
    }
    return false;
}

bool SetupLock::comeBackLater()
{
    wchar_t exe[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
    // Handed over in variables: a & or % in a file name stays part of the name.
    SetEnvironmentVariableW(L"ECHOPLAY_EXE", exe);
    SetEnvironmentVariableW(L"ECHOPLAY_ARGS", ownArguments().c_str());
    // PowerShell watches the mutexes, because an EchoPlay started just to look would hold the very files the setup is
    // replacing. Three minutes at most: a setup that hangs must not lose the clip.
    const std::wstring script = std::wstring(L"$t=[DateTime]::Now.AddMinutes(3);do{Start-Sleep -Milliseconds 200;$h=$false;foreach($n in '")
        + SetupMutex + L"','" + StartMutex
        + L"'){$m=$null;if([Threading.Mutex]::TryOpenExisting($n,[ref]$m)){$m.Close();$h=$true}}}while($h -and [DateTime]::Now -lt $t);"
          L"$i=New-Object Diagnostics.ProcessStartInfo $env:ECHOPLAY_EXE,$env:ECHOPLAY_ARGS;$i.UseShellExecute=$false;[void][Diagnostics.Process]::Start($i)";
    return start(systemFile(L"WindowsPowerShell\\v1.0\\powershell.exe"),
                 L"powershell.exe -NoProfile -NonInteractive -Command \"" + script + L"\"", FALSE, CREATE_NO_WINDOW);
}

bool SetupLock::startSetup(const QString &path, bool reopen)
{
    if (GetSystemMetrics(SM_SHUTTINGDOWN)) return false;
    SECURITY_ATTRIBUTES inheritable{sizeof inheritable, nullptr, TRUE};
    HANDLE lock = CreateMutexW(&inheritable, FALSE, StartMutex); // the setup inherits it and holds it to its end
    const std::wstring setup = QDir::toNativeSeparators(path).toStdWString();
    std::wstring command = quoteArgument(setup) + L" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
    if (reopen) command += L" /ECHOPLAYUPDATE=1"; // installer/windows/v2/EchoPlay.iss
    const bool started = start(setup, command, TRUE, 0);
    if (lock) CloseHandle(lock);
    return started;
}

#else

bool SetupLock::isHeld() { return false; }
bool SetupLock::comeBackLater() { return false; }
bool SetupLock::startSetup(const QString &, bool) { return false; }

#endif
