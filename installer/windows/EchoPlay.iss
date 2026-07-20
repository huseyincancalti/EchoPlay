; EchoPlay installer.
; Built by .github/workflows/release.yml, which first populates
; installer\windows\dist\ with a branded, renamed mpv engine (EchoPlay.exe)
; plus uosc/thumbfast/memo, then runs `iscc EchoPlay.iss` to produce
; EchoPlay-Setup.exe.
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
SetupIconFile=..\..\assets\logo.ico
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

; thumbfast spawns a background "mpv" subprocess to render thumbnails and
; defaults to looking for that literal name; Windows resolves an unqualified
; exe name by checking the calling app's own directory first, so shipping a
; second copy of the same (already-branded) binary under the literal name
; "mpv.exe" alongside EchoPlay.exe lets thumbfast find it with zero extra
; config. Nothing is ever registered/shortcut-launched under this name -
; Explorer, Task Manager and "Open With" only ever see EchoPlay.exe.
Source: "dist\mpv\EchoPlay.exe"; DestDir: "{app}"; DestName: "mpv.exe"; Flags: ignoreversion

; EchoPlay's own config (mpv.conf, input.conf, scripts, language packs, themes, uosc.conf).
Source: "..\..\mpv\*"; DestDir: "{app}\portable_config"; Flags: recursesubdirs ignoreversion

; uosc UI engine.
Source: "dist\uosc\scripts\*"; DestDir: "{app}\portable_config\scripts"; Flags: recursesubdirs ignoreversion
Source: "dist\uosc\fonts\*"; DestDir: "{app}\portable_config\fonts"; Flags: recursesubdirs ignoreversion

; thumbfast (hover thumbnails) + memo (watch history / resume).
Source: "dist\thumbfast.lua"; DestDir: "{app}\portable_config\scripts"; Flags: ignoreversion
Source: "dist\memo.lua"; DestDir: "{app}\portable_config\scripts"; Flags: ignoreversion
Source: "dist\memo.conf"; DestDir: "{app}\portable_config\script-opts"; Flags: ignoreversion skipifsourcedoesntexist

; Logo, used for shortcuts and the "Open with" entry icon.
Source: "..\..\assets\logo.ico"; DestDir: "{app}"; Flags: ignoreversion

; GPL-3.0/LGPL-2.1/MPL-2.0 notices for uosc/memo/thumbfast/mpv.
Source: "..\..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

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

; NOTE: a Capabilities/RegisteredApplications registration (the "Default Programs" mechanism)
; was tried here to let "Set as Default App" deep-link straight to EchoPlay's own page in Windows
; Settings, but was reverted after live testing: on real Windows 11, the Default Apps search/list
; only recognizes apps registered under HKLM (every real app checked - Adobe Acrobat, VLC,
; PotPlayer - registers there), not HKCU. HKLM would require admin, which conflicts with this
; installer's no-UAC design. "Set as Default App" instead opens the plain ms-settings:defaultapps
; page and tells the user to click "Choose defaults by file type".

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch EchoPlay"; Flags: postinstall nowait skipifsilent unchecked

[Messages]
FinishedLabel=Setup has finished installing [name] on your computer.%n%nTo make EchoPlay your default player: right-click any video or audio file, choose "Open with" -> "EchoPlay" -> "Always".

[UninstallDelete]
Type: filesandordirs; Name: "{app}\portable_config\watch_later"
Type: filesandordirs; Name: "{app}\portable_config\echoplay-state.json"
