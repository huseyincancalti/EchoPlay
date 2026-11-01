# Run by tools/media-keys.sh. One question to Windows per run, as a process that keeps asking is not told of changes (it
# has no message loop). EchoPlay's media session only; other apps' sessions are not touched.
# status: prints "<playback status>|<title>", or nothing without a session. toggle: sends play/pause, what a keyboard's
# media key, a headset's button and the media overlay do.
param([string]$Action)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$asTask = [System.WindowsRuntimeSystemExtensions].GetMethods() |
    Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' } |
    Select-Object -First 1
function Await($operation, $type) {
    $task = $asTask.MakeGenericMethod($type).Invoke($null, @($operation))
    [void]$task.Wait(5000)
    $task.Result
}
$managerType = [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager, Windows.Media.Control, ContentType = WindowsRuntime]
$propertiesType = [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionMediaProperties, Windows.Media.Control, ContentType = WindowsRuntime]
$manager = Await ($managerType::RequestAsync()) $managerType
$session = $manager.GetSessions() | Where-Object { $_.SourceAppUserModelId -match 'EchoPlay' } | Select-Object -First 1
if (-not $session) { exit 0 }
if ($Action -eq 'toggle') { [void](Await ($session.TryTogglePlayPauseAsync()) ([bool])); exit 0 }
$properties = Await ($session.TryGetMediaPropertiesAsync()) $propertiesType
Write-Output "$($session.GetPlaybackInfo().PlaybackStatus)|$($properties.Title)"
