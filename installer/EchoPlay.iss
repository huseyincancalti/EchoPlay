; EchoPlay installer.
; Built by .github/workflows/release.yml, which first populates installer\dist\
; with a branded, renamed mpv engine (EchoPlay.exe) plus uosc/thumbfast/memo,
; then runs `iscc EchoPlay.iss` to produce EchoPlay-Setup.exe.
;
; mpv.exe is renamed to EchoPlay.exe (and re-branded with rcedit) before this
; script runs, so every Windows surface - Open With, taskbar, Task Manager,
; the uninstaller entry - shows "EchoPlay", never "mpv".

#define MyAppName "EchoPlay"
#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#define MyAppPublisher "Huseyin Can CALTI"
#define MyAppURL "https://github.com/huseyincancalti/EchoPlay"
#define MyAppExeName "EchoPlay.exe"

[Setup]
AppId={{6E9F2C9C-6E61-4A8B-9A5B-2F1E0E6F4C2D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={localappdata}\Programs\EchoPlay
DefaultGroupName=EchoPlay
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=Output
OutputBaseFilename=EchoPlay-Setup
SetupIconFile=..\assets\logo.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion={#MyAppVersion}
VersionInfoProductName={#MyAppName}
VersionInfoCompany={#MyAppPublisher}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; mpv engine, already renamed to EchoPlay.exe and branded by the CI workflow.
Source: "dist\mpv\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

; EchoPlay's own config (mpv.conf, input.conf, scripts, language packs, themes, uosc.conf).
Source: "..\mpv\*"; DestDir: "{app}\portable_config"; Flags: recursesubdirs ignoreversion

; uosc UI engine.
Source: "dist\uosc\scripts\*"; DestDir: "{app}\portable_config\scripts"; Flags: recursesubdirs ignoreversion
Source: "dist\uosc\fonts\*"; DestDir: "{app}\portable_config\fonts"; Flags: recursesubdirs ignoreversion

; thumbfast (hover thumbnails) + memo (watch history / resume).
Source: "dist\thumbfast.lua"; DestDir: "{app}\portable_config\scripts"; Flags: ignoreversion
Source: "dist\memo.lua"; DestDir: "{app}\portable_config\scripts"; Flags: ignoreversion
Source: "dist\memo.conf"; DestDir: "{app}\portable_config\script-opts"; Flags: ignoreversion skipifsourcedoesntexist

; Logo, used for shortcuts and the "Open with" entry icon.
Source: "..\assets\logo.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\EchoPlay"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\logo.ico"
Name: "{group}\{cm:UninstallProgram,EchoPlay}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\EchoPlay"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\logo.ico"; Tasks: desktopicon

[Registry]
; Register EchoPlay.exe as an "Open with" application under its own (renamed)
; identity - this is what makes the dialog show "EchoPlay", not "mpv".
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#MyAppName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\logo.ico"
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

#define VideoExts ".mp4,.mkv,.avi,.mov,.webm,.m4v,.wmv,.ts,.flv,.3gp,.rmvb,.ogm"
#define AudioExts ".mp3,.flac,.m4a,.wav,.ogg,.opus,.aac,.wma,.wv"

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
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mp3"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".flac"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".m4a"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wav"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ogg"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".opus"; ValueData: ""

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch EchoPlay"; Flags: postinstall nowait skipifsilent unchecked

[Messages]
FinishedLabel=Setup has finished installing [name] on your computer.%n%nTo make EchoPlay your default player: right-click any video or audio file, choose "Open with" -> "EchoPlay" -> "Always".

[UninstallDelete]
Type: filesandordirs; Name: "{app}\portable_config\watch_later"
Type: filesandordirs; Name: "{app}\portable_config\echoplay-state.json"
