# For tools/deps-check.sh: one line per DLL name in the list file, "<name><tab>windows" when System32 has it and it is
# signed as part of the operating system, "<name><tab>NOT-WINDOWS" when another program put it there (a redistributable
# is signed by its publisher, not as an OS file), "<name><tab>MISSING" when System32 does not have it.
param([Parameter(Mandatory = $true)][string]$List)
Get-Content $List | Where-Object { $_ } | ForEach-Object {
    $path = Join-Path $env:WINDIR "System32\$_"
    if (-not (Test-Path $path)) { "$_`tMISSING" }
    elseif ((Get-AuthenticodeSignature $path).IsOSBinary) { "$_`twindows" }
    else { "$_`tNOT-WINDOWS" }
}
