; EchoPlay v2 installer (Qt + libmpv app). Built by tools/package-v2.sh, which stages the app into
; installer\windows\v2\stage\ (cmake --install) and then runs ISCC on this script.
;
; Same AppId, install folder and "Open with" registration as v1 (installer\windows\EchoPlay.iss): installing
; v2 replaces v1 in place, and v1's engine files and config are removed ([InstallDelete]). v2 starts with fresh
; settings (maintainer, 2026-10-31). Per-user, no admin: nothing to set up before or after.

#define MyAppName "EchoPlay"
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0-dev"
#endif
#ifndef MyFileVersion
  #define MyFileVersion "0.0.0"
#endif
#define MyAppPublisher "Huseyin Can CALTI"
#define MyAppURL "https://github.com/huseyincancalti/EchoPlay"
#define MyAppExeName "EchoPlay.exe"

[Setup]
#ifdef TestDir
; tools/setup-live.sh: the same setup installing into a test folder: its own AppId, no uninstall entry, no shortcuts,
; no registry and no v1 cleanup, so it cannot touch the EchoPlay installed on the machine.
AppId={{B7A3F1D4-2C51-4E0A-9F3B-7D1C5A6E8F20}
DefaultDirName={#TestDir}
Uninstallable=no
#else
AppId={{6E9F2C9C-6E61-4A8B-9A5B-2F1E0E6F4C2D}
DefaultDirName={localappdata}\Programs\EchoPlay
#endif
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultGroupName=EchoPlay
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=Output
OutputBaseFilename=EchoPlay-Setup
SetupIconFile=..\..\..\assets\logo.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion={#MyFileVersion}
VersionInfoProductName={#MyAppName}
VersionInfoCompany={#MyAppPublisher}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; A running EchoPlay holds its files: ask to close it, as any update of a running app needs.
CloseApplications=yes
; One setup at a time. The lock a new EchoPlay looks at is CurStepChanged's, below.
SetupMutex=EchoPlaySetup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

#ifndef TestDir
[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[InstallDelete]
; v1 leftovers: its mpv copy for thumbfast, and its whole mpv config (scripts, uosc, state, watch_later).
Type: files; Name: "{app}\mpv.exe"
Type: filesandordirs; Name: "{app}\portable_config"
#endif

[Files]
; The staged app: EchoPlay.exe, Qt and MinGW runtime, QML modules, libmpv, lang\, theme.json.
Source: "stage\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
Source: "..\..\..\assets\logo.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\..\app\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\..\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion

#ifndef TestDir
[Icons]
Name: "{group}\EchoPlay"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\logo.ico"
Name: "{group}\{cm:UninstallProgram,EchoPlay}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\EchoPlay"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\logo.ico"; Tasks: desktopicon

[Registry]
; The same "Open with" registration as v1 (see installer\windows\EchoPlay.iss for why each line exists).
Root: HKCU; Subkey: "Software\Classes\EchoPlay.Media"; Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#MyAppName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\logo.ico"
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mp4"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mkv"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".avi"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mov"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".webm"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".m4v"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wmv"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ts"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".flv"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".3gp"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".rmvb"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ogm"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mp3"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".flac"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".m4a"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wav"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ogg"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".opus"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".aac"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wma"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wv"; ValueData: ""
#endif

[Run]
; --after-setup: this setup's own lock does not apply, the files are in place.
Filename: "{app}\{#MyAppExeName}"; Parameters: "--after-setup"; Description: "Launch EchoPlay"; Flags: postinstall nowait skipifsilent unchecked
; An update started with Install from inside EchoPlay (/ECHOPLAYUPDATE=1) reopens it, on the start screen. One that
; installs when EchoPlay closes does not: it was closed on purpose.
Filename: "{app}\{#MyAppExeName}"; Parameters: "--after-setup"; Flags: nowait; Check: IsAppUpdate

[Code]
function IsAppUpdate: Boolean;
begin
  Result := ExpandConstant('{param:ECHOPLAYUPDATE|0}') = '1';
end;

// While this mutex exists a new EchoPlay leaves at once and comes back when it is gone (app/src/SetupLock.cpp). From
// here, not from the start: a wizard may stay open for as long as someone likes, with EchoPlay opening as usual.
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then CreateMutex('EchoPlayInstalling');
end;

[Messages]
FinishedLabel=Setup has finished installing [name] on your computer.%n%nTo make EchoPlay your default player: right-click any video or audio file, choose "Open with" -> "EchoPlay" -> "Always".
