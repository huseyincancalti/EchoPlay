#include "SetupLock.h"

#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

namespace {

constexpr wchar_t InstallingMutex[] = L"EchoPlayInstalling"; // CurStepChanged in installer/windows/v2/EchoPlay.iss
// The one hold takes. It cannot be the setup's own SetupMutex: Inno Setup quits without a word when that already exists.
constexpr wchar_t StartMutex[] = L"EchoPlaySetupStarting";
HANDLE held = nullptr; // hold's, until startSetup hands it on

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

// inherit: the one handle the new process gets, none when null. Anything else of EchoPlay's that happens to be
// inheritable would stay open for as long as the setup runs.
bool start(const std::wstring &program, std::wstring command, DWORD flags, HANDLE inherit = nullptr)
{
    STARTUPINFOEXW info{};
    info.StartupInfo.cb = sizeof info;
    std::vector<char> list;
    if (inherit) {
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        list.resize(size);
        info.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(list.data());
        if (!InitializeProcThreadAttributeList(info.lpAttributeList, 1, 0, &size)
            || !UpdateProcThreadAttribute(info.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &inherit, sizeof inherit,
                                          nullptr, nullptr))
            return false;
        flags |= EXTENDED_STARTUPINFO_PRESENT;
    }
    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessW(program.c_str(), command.data(), nullptr, nullptr, inherit != nullptr, flags, nullptr,
                                        nullptr, &info.StartupInfo, &process);
    if (inherit) DeleteProcThreadAttributeList(info.lpAttributeList);
    if (!started) return false;
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
    for (const wchar_t *name : {InstallingMutex, StartMutex}) {
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
    // A hidden process cannot bring a window to the front. This launch, just started by the user, may pass that on, and
    // it ends at their next click or key: a clip does not jump over what they went on to.
    AllowSetForegroundWindow(ASFW_ANY);
    // PowerShell watches the mutexes, because an EchoPlay started just to look would hold the very files the setup is
    // replacing. Three minutes at most: a setup that hangs must not lose the clip. A locked-down PowerShell (Constrained
    // Language Mode) refuses the mutex call but not Start-Process: 30 s is longer than the setup takes.
    const std::wstring script = std::wstring(L"try{$t=[DateTime]::Now.AddMinutes(3);do{Start-Sleep -Milliseconds 200;$h=$false;foreach($n in '")
        + InstallingMutex + L"','" + StartMutex
        + L"'){$m=$null;if([Threading.Mutex]::TryOpenExisting($n,[ref]$m)){$m.Close();$h=$true}}}while($h -and [DateTime]::Now -lt $t)}catch{Start-Sleep 30};"
          L"Start-Process -FilePath $env:ECHOPLAY_EXE -ArgumentList $env:ECHOPLAY_ARGS";
    return start(systemFile(L"WindowsPowerShell\\v1.0\\powershell.exe"),
                 L"powershell.exe -NoProfile -NonInteractive -Command \"" + script + L"\"", CREATE_NO_WINDOW);
}

void SetupLock::hold()
{
    if (held) return;
    SECURITY_ATTRIBUTES inheritable{sizeof inheritable, nullptr, TRUE};
    held = CreateMutexW(&inheritable, FALSE, StartMutex);
}

bool SetupLock::startSetup(const QString &path, bool reopen)
{
    if (GetSystemMetrics(SM_SHUTTINGDOWN)) return false;
    hold(); // the setup inherits it and holds it to its end
    const std::wstring setup = QDir::toNativeSeparators(path).toStdWString();
    std::wstring command = quoteArgument(setup) + L" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
    if (reopen) command += L" /ECHOPLAYUPDATE=1"; // installer/windows/v2/EchoPlay.iss
    const bool started = start(setup, command, 0, held);
    if (held) CloseHandle(held);
    held = nullptr;
    return started;
}

#else

bool SetupLock::isHeld() { return false; }
bool SetupLock::comeBackLater() { return false; }
void SetupLock::hold() {}
bool SetupLock::startSetup(const QString &, bool) { return false; }

#endif
