[CmdletBinding()]
param(
    [ValidateSet('mixed','humans')]
    [string]$Mode = 'mixed',
    [ValidateRange(1024,65535)]
    [int]$Port = 17891,
    [string]$EvidenceDirectory = '',
    [ValidateRange(0,14)][int]$Map = 2
)

$ErrorActionPreference = 'Stop'
$revision = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$exe = (Resolve-Path -LiteralPath (Join-Path $revision 'Builds/Windows/Chronicles/Binaries/Win64/Chronicles.exe')).Path
$logRoot = Join-Path $revision 'Game/Saved/BuildLogs'
$savedTests = Join-Path $revision 'Builds/Windows/Chronicles/Saved/Tests'
$evidence = if ($EvidenceDirectory) { [IO.Path]::GetFullPath($EvidenceDirectory) } else { Join-Path $revision "Docs/Evidence/v0.9/network/$Mode" }
$roles = if ($Mode -eq 'mixed') { @('host','guest') } else { @('host','client1','client2','client3') }
$testName = if ($Mode -eq 'mixed') { 'network-v09-bots' } else { 'network' }
$processes = [ordered]@{}
$started = Get-Date

if (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue) {
    throw "The LAN test port $Port is in use."
}
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null

try {
    foreach ($role in $roles) {
        $url = if ($role -eq 'host') { '/Engine/Maps/Entry?listen' } else { "127.0.0.1:$Port" }
        $log = Join-Path $logRoot "Test-LAN-v09-$Mode-$role.log"
        $probe = if ($Mode -eq 'mixed') { @("-RTSNetV09Bots=$role", "-RTSNetMap=$Map") } else { @("-RTSNetTest=$role",'-RTSNetPlayers=4') }
        $arguments = @($url,"-port=$Port",'-nullrhi','-unattended','-nosound',('-abslog="' + $log + '"')) + $probe
        $processes[$role] = Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
        if ($role -eq 'host') {
            $hostDeadline = (Get-Date).AddSeconds(40)
            while (-not (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue)) {
                if ($processes['host'].HasExited -or (Get-Date) -ge $hostDeadline) {
                    throw "The listen server did not open port $Port. See $log"
                }
                Start-Sleep -Milliseconds 500
            }
        }
    }
    Write-Output "Started $($processes.Count) separate $Mode LAN test processes on port $Port."

    $deadline = (Get-Date).AddSeconds(160)
    while (@($processes.Values | Where-Object { -not $_.HasExited }).Count -gt 0) {
        if ((Get-Date) -ge $deadline) { throw "LAN test timed out. See $logRoot" }
        Start-Sleep -Seconds 1
    }
    foreach ($role in $roles) {
        $process = $processes[$role]
        if ($process.ExitCode -ne 0) {
            throw "LAN test $role exited with code $($process.ExitCode). See $logRoot"
        }
        $reportFile = Get-Item -LiteralPath (Join-Path $savedTests "$testName-$role.json")
        if ($reportFile.LastWriteTime -lt $started) { throw "Stale report for $role." }
        $report = Get-Content -LiteralPath $reportFile.FullName -Raw | ConvertFrom-Json
        if (-not $report.passed) { throw "LAN assertions failed for $role." }
        New-Item -ItemType Directory -Path $evidence -Force | Out-Null
        Copy-Item -LiteralPath $reportFile.FullName -Destination $evidence -Force
        Copy-Item -LiteralPath (Join-Path $logRoot "Test-LAN-v09-$Mode-$role.log") -Destination $evidence -Force
    }
    if($Mode -eq 'mixed'){
        $hostReport=Get-Content -LiteralPath (Join-Path $evidence 'network-v09-bots-host.json') -Raw|ConvertFrom-Json
        $guestReport=Get-Content -LiteralPath (Join-Path $evidence 'network-v09-bots-guest.json') -Raw|ConvertFrom-Json
        if($hostReport.map -ne $Map -or $guestReport.map -ne $Map -or $hostReport.terrain_signature -ne $guestReport.terrain_signature){throw 'LAN map terrain differs between host and client'}
    }
    [ordered]@{started=$started.ToUniversalTime().ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');sha256=(Get-FileHash -LiteralPath $exe).Hash;mode=$Mode;map=if($Mode -eq 'mixed'){$Map}else{$null};port=$Port;roles=$roles;passed=$true}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $evidence 'run.json') -Encoding utf8
    Write-Output "All $Mode LAN processes exited successfully. Evidence is in $evidence"
} finally {
    foreach ($process in $processes.Values) {
        if (-not $process.HasExited) {
            $current = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
            if ($current -and $current.Path -eq $exe) { Stop-Process -Id $current.Id }
        }
    }
}
