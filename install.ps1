<#
EchoPlay installer - zero-friction setup.
  - If mpv isn't installed, downloads a portable mpv build automatically.
  - Copies EchoPlay config into the mpv config directory.
  - Downloads upstream components (uosc, thumbfast, memo) into the same directory.
  - Patches uosc controls to show the EchoPlay audio button.
  - Rebrands mpv.exe to EchoPlay and registers it for the Windows "Open with" list.

Usage:
  powershell -ExecutionPolicy Bypass -File install.ps1
  [-ConfigDir <path>] [-Portable] [-SkipAssoc] [-SkipMpvDownload]
#>
param(
    [string]$ConfigDir = "",
    [switch]$Portable,
    [switch]$SkipAssoc,
    [switch]$SkipMpvDownload
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src  = Join-Path $root 'mpv'
$UOSC_VERSION = '5.12.0'

function Info($m) { Write-Host "[EchoPlay] $m" -ForegroundColor Cyan }
function Warn($m) { Write-Host "[EchoPlay] $m" -ForegroundColor Yellow }

# Standalone 7-Zip extractor (PowerShell's Expand-Archive can't read .7z).
function Get-SevenZip {
    $z = Join-Path $env:TEMP 'echoplay-7zr.exe'
    if (-not (Test-Path $z)) {
        Invoke-WebRequest -UseBasicParsing -Uri 'https://www.7-zip.org/a/7zr.exe' -OutFile $z
    }
    return $z
}

# Download a portable mpv build (shinchiro builds mirrored on GitHub with direct
# download URLs) and extract it into $Dest. Returns the path to mpv.exe.
function Install-PortableMpv {
    param([string]$Dest)
    Info "mpv bulunamadi -> tasinabilir mpv indiriliyor (tek seferlik, ~32 MB)..."
    $rel = Invoke-RestMethod -UseBasicParsing -Headers @{ 'User-Agent' = 'EchoPlay' } `
        -Uri 'https://api.github.com/repos/zhongfly/mpv-winbuild/releases/latest'
    # baseline x86_64 build (the -v3- variant needs a newer CPU with AVX2)
    $asset = $rel.assets | Where-Object { $_.name -match '^mpv-x86_64-[0-9].*\.7z$' } | Select-Object -First 1
    if (-not $asset) { throw "mpv build asseti bulunamadi" }
    $arc = Join-Path $env:TEMP $asset.name
    Info "Indiriliyor: $($asset.name)"
    Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $arc
    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
    & (Get-SevenZip) x "-o$Dest" $arc -y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "mpv arsivi cikartilamadi" }
    Remove-Item $arc -Force -ErrorAction SilentlyContinue
    $exe = Get-ChildItem -Path $Dest -Recurse -Filter 'mpv.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $exe) { throw "mpv.exe cikartilan dosyalarda bulunamadi" }
    Info "Tasinabilir mpv kuruldu: $($exe.FullName)"
    return $exe.FullName
}

# --- 1. Locate mpv.exe (download a portable build if missing) ---------------
$mpv = (Get-Command mpv -ErrorAction SilentlyContinue).Source
if (-not $mpv) {
    $guess = "$env:LOCALAPPDATA\Programs\mpv\mpv.exe"
    if (Test-Path $guess) { $mpv = $guess }
}
if (-not $mpv -and -not $SkipMpvDownload) {
    try {
        $mpv = Install-PortableMpv -Dest (Join-Path $env:LOCALAPPDATA 'Programs\EchoPlay')
        $Portable = $true   # use a self-contained portable_config next to the new mpv.exe
    } catch { Warn "Otomatik mpv kurulumu basarisiz: $($_.Exception.Message)" }
}
if (-not $ConfigDir) {
    if ($env:MPV_HOME) {
        $ConfigDir = $env:MPV_HOME
    } elseif ($mpv) {
        $pc = Join-Path (Split-Path -Parent $mpv) 'portable_config'
        # mpv reads portable_config (next to mpv.exe) in preference to %APPDATA%\mpv.
        # scoop creates this as a persisted junction, so detect & use it automatically.
        if ($Portable -or (Test-Path $pc)) { $ConfigDir = $pc }
        else { $ConfigDir = Join-Path $env:APPDATA 'mpv' }
    } else {
        $ConfigDir = Join-Path $env:APPDATA 'mpv'
    }
}
Info "mpv: $(if($mpv){$mpv}else{'(bulunamadi)'})"
Info "Config dizini: $ConfigDir"
New-Item -ItemType Directory -Force -Path $ConfigDir, "$ConfigDir\scripts", "$ConfigDir\script-opts", "$ConfigDir\fonts" | Out-Null

# --- 2. Copy EchoPlay files -------------------------------------------------
Copy-Item -Path (Join-Path $src '*') -Destination $ConfigDir -Recurse -Force
Info "EchoPlay dosyalari kopyalandi."

$tmp = Join-Path $env:TEMP ("echoplay_" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
try {
    # --- 3. uosc ------------------------------------------------------------
    try {
        Info "uosc $UOSC_VERSION indiriliyor..."
        $zip = Join-Path $tmp 'uosc.zip'
        Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/tomasklaen/uosc/releases/download/$UOSC_VERSION/uosc.zip" -OutFile $zip
        $ux = Join-Path $tmp 'uosc'
        Expand-Archive -Path $zip -DestinationPath $ux -Force
        foreach ($d in 'scripts','fonts') {
            if (Test-Path "$ux\$d") { Copy-Item "$ux\$d\*" "$ConfigDir\$d" -Recurse -Force }
        }
        $uconf = "$ConfigDir\script-opts\uosc.conf"
        if (-not (Test-Path $uconf)) {
            Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/tomasklaen/uosc/releases/download/$UOSC_VERSION/uosc.conf" -OutFile $uconf
        }
        # EchoPlay uosc tweaks (idempotent): centered transport cluster (rewind /
        # play-pause / forward) balanced by the EchoPlay button and fullscreen,
        # rounder corners, slightly bolder timeline line. Volume = right-edge slider.
        $c = Get-Content $uconf -Raw
        $c = $c -replace '(?m)^#?controls=.*$', 'controls=button:echoplay,space,command:replay_10:seek -10,play-pause,command:forward_10:seek 10,space,fullscreen'
        $c = $c -replace '(?m)^#?border_radius=.*$', 'border_radius=10'
        $c = $c -replace '(?m)^#?timeline_line_width=.*$', 'timeline_line_width=3'
        $c = $c -replace '(?m)^#?timeline_size=.*$', 'timeline_size=32'
        $c = $c -replace '(?m)^#?color=.*$', 'color=foreground=ee7733'
        $c = $c -replace '(?m)^#?languages=.*$', 'languages=tr,slang,en'
        Set-Content -Path $uconf -Value $c -Encoding UTF8
        Info "uosc kuruldu + EchoPlay butonu eklendi."
    } catch { Warn "uosc kurulamadi: $($_.Exception.Message)" }

    # --- 4. thumbfast -------------------------------------------------------
    try {
        Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/po5/thumbfast/master/thumbfast.lua' -OutFile "$ConfigDir\scripts\thumbfast.lua"
        Info "thumbfast kuruldu."
    } catch { Warn "thumbfast kurulamadi: $($_.Exception.Message)" }

    # --- 5. memo (history / continue watching) ------------------------------
    try {
        Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/po5/memo/master/memo.lua' -OutFile "$ConfigDir\scripts\memo.lua"
        if (-not (Test-Path "$ConfigDir\script-opts\memo.conf")) {
            Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/po5/memo/master/memo.conf' -OutFile "$ConfigDir\script-opts\memo.conf"
        }
        Info "memo kuruldu."
    } catch { Warn "memo kurulamadi: $($_.Exception.Message)" }
} finally {
    Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
}

# --- 6. Register EchoPlay in the Windows "Open with" list (no UAC, branded).
if (-not $SkipAssoc -and $mpv) {
    try {
        # Register mpv.exe (GUI subsystem) as a proper app so it shows up in the
        # Windows "Open with" list as "EchoPlay". NOT mpv.com / a console shim,
        # which are console subsystem and pop a terminal behind the player.
        $mpvGui = $mpv -replace '\.com$', '.exe'
        if (-not (Test-Path $mpvGui)) { $mpvGui = $mpv }
        $base = 'HKCU:\Software\Classes\Applications\mpv.exe'
        New-Item -Path "$base\shell\open\command" -Force | Out-Null
        Set-ItemProperty -Path "$base\shell\open\command" -Name '(default)' -Value "`"$mpvGui`" `"%1`""
        Set-ItemProperty -Path $base -Name 'FriendlyAppName' -Value 'EchoPlay'
        $icon = "$mpvGui,0"
        $logo = Join-Path $root 'assets\logo.ico'
        if (Test-Path $logo) { Copy-Item $logo (Join-Path $ConfigDir 'logo.ico') -Force; $icon = Join-Path $ConfigDir 'logo.ico' }
        New-Item -Path "$base\DefaultIcon" -Force | Out-Null
        Set-ItemProperty -Path "$base\DefaultIcon" -Name '(default)' -Value $icon
        New-Item -Path "$base\SupportedTypes" -Force | Out-Null
        $exts = '.mp4','.mkv','.avi','.mov','.webm','.m4v','.wmv','.ts','.flv','.3gp','.rmvb','.ogm','.mp3','.flac','.m4a','.wav','.ogg','.opus','.aac','.wma','.wv'
        foreach ($ext in $exts) {
            Set-ItemProperty -Path "$base\SupportedTypes" -Name $ext -Value ''
        }
        Info "Player 'Birlikte ac' listesine 'EchoPlay' olarak eklendi."
        Warn "Varsayilan yapmak icin: videoya sag tik > Birlikte ac > 'EchoPlay' > 'Her zaman'."
        # NOTE: a Capabilities/RegisteredApplications ("Default Programs") registration was tried
        # here so EchoPlay could deep-link straight to its own page in Windows Settings, but was
        # reverted after live testing: Windows 11's Default Apps search only recognizes apps
        # registered under HKLM (every real app checked - Adobe Acrobat, VLC, PotPlayer - is HKLM),
        # not HKCU, and HKLM needs admin. EchoPlay's in-app "Set as Default App" instead opens the
        # plain ms-settings:defaultapps page and points the user at "Choose defaults by file type".
    } catch { Warn "Dosya iliskilendirme atlandi: $($_.Exception.Message)" }
}

# --- 7. Rebrand mpv.exe to EchoPlay: embed the logo (window / taskbar / exe icon)
#        and set the version strings so Windows shows "EchoPlay", not "mpv".
#        Reverts when scoop updates mpv -> just re-run this installer.
if (-not $SkipAssoc -and $mpv) {
    try {
        $mpvGui = $mpv -replace '\.com$', '.exe'
        if (-not (Test-Path $mpvGui)) { $mpvGui = $mpv }
        $rc = Join-Path $env:TEMP 'rcedit-x64.exe'
        if (-not (Test-Path $rc)) {
            Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/electron/rcedit/releases/download/v2.0.0/rcedit-x64.exe' -OutFile $rc
        }
        Get-Process -Name mpv -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 600
        $logoIco = Join-Path $root 'assets\logo.ico'
        if (Test-Path $logoIco) { & $rc $mpvGui --set-icon $logoIco }
        # FileDescription drives the taskbar tooltip / Task Manager / "Open with" name.
        & $rc $mpvGui --set-version-string 'FileDescription' 'EchoPlay' `
                      --set-version-string 'ProductName' 'EchoPlay' `
                      --set-version-string 'CompanyName' 'EchoPlay'
        if ($LASTEXITCODE -eq 0) { Info "mpv.exe -> EchoPlay olarak markalandi (ikon + isim)." }
        else { Warn "exe markalanamadi (mpv acik olabilir; kapatip installer'i tekrar calistirin)." }
    } catch { Warn "Rebrand atlandi: $($_.Exception.Message)" }
}

Info "Kurulum tamam. Bir video acip sag tik (veya 'a') ile EchoPlay Ayarlar menusunu kullan."
