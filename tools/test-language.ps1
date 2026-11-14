param(
    [string]$InstallDir = "$env:LOCALAPPDATA\Programs\EchoPlay",
    [switch]$Preview
)
$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
    $mpv = Join-Path $InstallDir 'mpv.exe'
    if (!(Test-Path -LiteralPath $mpv)) { throw "mpv.exe not found in $InstallDir" }
    New-Item -ItemType Directory -Force '.out' | Out-Null
    if (!$Preview) {
        $testProcess = Start-Process -FilePath $mpv -WindowStyle Hidden -PassThru -Wait -ArgumentList @(
            '--no-config', '--idle=yes', '--vo=null', '--ao=null',
            '--script=tools/test-language-mpv.lua', '--log-file=.out/language-tests.log'
        )
        Get-Content '.out/language-tests.log' | Select-String 'ok   |FAIL |passed|exit code'
        if ($testProcess.ExitCode -ne 0) { throw 'Language tests failed; see .out/language-tests.log' }
    }

    # Use a separate portable configuration; never modify the installed copy.
    $previewDir = Join-Path (Get-Location) '.out\language-preview'
    if (!(Test-Path -LiteralPath $previewDir)) {
        Copy-Item -LiteralPath (Join-Path $InstallDir 'portable_config') -Destination $previewDir -Recurse
    }
    Copy-Item 'mpv/scripts/echoplay-audio/*.lua' (Join-Path $previewDir 'scripts\echoplay-audio') -Force
    $playerArgs = @("--config-dir=`"$previewDir`"", '--idle=no', '--keep-open=no',
        '--log-file=.out/language-smoke.log', 'av://lavfi:testsrc=duration=2')
    if ($Preview) {
        $playerArgs = @("--config-dir=`"$previewDir`"", '--idle=yes', '--force-window=yes',
            '--log-file=.out/language-preview.log')
        Start-Process -FilePath $mpv -ArgumentList $playerArgs | Out-Null
        Write-Output "Preview config: $previewDir"
    } else {
        $playerArgs += '--vo=null', '--ao=null'
        $smoke = Start-Process -FilePath $mpv -ArgumentList $playerArgs -WindowStyle Hidden -PassThru
        # EchoPlay restores keep-open at startup; bound the run like CI's timeout.
        $ended = $smoke.WaitForExit(8000)
        if (!$ended) { $smoke.Kill(); $smoke.WaitForExit() }
        if (($ended -and $smoke.ExitCode -ne 0) -or (Select-String -Path '.out/language-smoke.log' -Pattern 'Lua error|stack traceback')) {
            throw 'Smoke test failed; see .out/language-smoke.log'
        }
        Write-Output 'Windows language tests and full-config smoke test passed.'
    }
} finally {
    Pop-Location
}
