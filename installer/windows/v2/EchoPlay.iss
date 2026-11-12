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
; This file is UTF-8 with a byte order mark: without the mark Inno Setup reads the name in the build PC's code page.
#define MyAppPublisher "Hüseyin Can ÇALTI"
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
; The same "Open with" registration as v1 (see installer\windows\EchoPlay.iss for why each line exists). The stale
; EchoPlay.Media ProgID v1 purges is purged first here too, then written fresh below for Default apps.
Root: HKCU; Subkey: "Software\Classes\EchoPlay.Media"; Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#MyAppName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\logo.ico"
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""
; The file types: app/src/MediaFiles.cpp's lists (tst_mediafiles checks that both blocks below match them).
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".3g2"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".3gp"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".asf"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".avi"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".divx"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".f4v"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".flv"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ivf"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".m2t"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".m2ts"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".m4v"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mj2"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mkv"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mov"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mp4"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mpeg"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mpg"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mts"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mxf"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ogv"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".rm"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".rmvb"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ts"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".vob"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".webm"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wmv"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".y4m"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".aac"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ac3"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".aiff"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ape"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".au"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".dff"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".dsf"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".dts"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".eac3"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".flac"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".m4a"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mka"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mp1"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mp2"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mp3"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mpc"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".oga"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ogg"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ogm"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".opus"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".tak"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".thd"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".tta"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wav"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wma"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wv"; ValueData: ""

; Windows' Default apps page for EchoPlay (Settings > Playback > Default player opens it: only the user may pick a
; default). Its ProgID, the types it can take, and its entry in RegisteredApplications.
Root: HKCU; Subkey: "Software\Classes\EchoPlay.Media"; ValueType: string; ValueName: ""; ValueData: "Media file"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\EchoPlay.Media\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\logo.ico"
Root: HKCU; Subkey: "Software\Classes\EchoPlay.Media\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""
Root: HKCU; Subkey: "Software\EchoPlay"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "{#MyAppName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Plays game clips with separate game and mic tracks, mixed live"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities"; ValueType: string; ValueName: "ApplicationIcon"; ValueData: "{app}\logo.ico"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3g2"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3gp"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".asf"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avi"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".divx"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".f4v"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flv"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ivf"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2t"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2ts"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4v"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mj2"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mkv"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mov"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp4"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpeg"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpg"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mts"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mxf"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogv"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rm"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rmvb"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ts"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".vob"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webm"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wmv"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".y4m"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aac"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ac3"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aiff"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ape"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".au"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dff"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dsf"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dts"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".eac3"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flac"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4a"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mka"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp1"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp2"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp3"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpc"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".oga"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogg"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogm"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".opus"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tak"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".thd"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tta"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wav"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wma"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\EchoPlay\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wv"; ValueData: "EchoPlay.Media"
Root: HKCU; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "EchoPlay"; ValueData: "Software\EchoPlay\Capabilities"; Flags: uninsdeletevalue
#endif

[Run]
; Windows checks new program files the first time they run: the first start after an install or an update took about
; 2 s longer (tools/first-run.sh: a 2 s clip 5.7 s against 3.4 s). A hidden --smoke run pays that here instead, and
; leaves the next start as fast as any other. --after-setup: this setup's own lock does not apply to it.
Filename: "{app}\{#MyAppExeName}"; Parameters: "--smoke --after-setup"; Flags: runhidden nowait
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
FinishedLabel=Setup has finished installing [name] on your computer.%n%nTo make EchoPlay your default player: EchoPlay > Settings > Playback > Default player, or right-click any video or audio file, choose "Open with" -> "EchoPlay" -> "Always".
