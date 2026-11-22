$ErrorActionPreference = 'Stop'
$revisionRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$testExe = Join-Path $revisionRoot 'Builds\Windows\Chronicles\Binaries\Win64\Chronicles.exe'
$logRoot = Join-Path $revisionRoot 'Game\Saved\BuildLogs'
$port = 17890
if (Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue) { throw "Test port $port is occupied." }
$runStarted = Get-Date
$processes = @()
$roles = @('host','client1','client2','client3')
for ($i=0; $i -lt 4; $i++) {
    $role = $roles[$i]
    $url = if ($i -eq 0) { '/Engine/Maps/Entry?listen' } else { "127.0.0.1:$port" }
    $args = @($url,"-port=$port","-RTSNetTest=$role",'-RTSNetPlayers=4','-RTSNetStress','-RTSNetV04','-RTSNetV05','-RTSNetV07','-RTSNetV08','-RenderOffscreen','-d3d11','-unattended','-windowed','-ForceRes','-ResX=800','-ResY=600','-nosound','-ExecCmds="t.MaxFPS 30"',"-abslog=`"$logRoot\Network-v08-$role.log`"")
    $processes += Start-Process -WindowStyle Hidden -PassThru -FilePath $testExe -ArgumentList $args
    if ($i -eq 0) { Start-Sleep -Seconds 2 }
}
@($processes | ForEach-Object { @{id=$_.Id;path=$testExe} }) | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $logRoot 'Network-v08-processes.json')
Write-Output "Four offscreen v0.8 clients started on test port $port."
$deadline = (Get-Date).AddSeconds(210)
while ((Get-Date) -lt $deadline -and @($processes | Where-Object { -not $_.HasExited }).Count) { Start-Sleep -Seconds 1 }
$result = @()
for ($i=0; $i -lt $processes.Count; $i++) {
    $process = $processes[$i]
    if (-not $process.HasExited) {
        $current = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if ($current -and $current.Path -eq $testExe) { Stop-Process -Id $current.Id }
        $result += @{role=$roles[$i];timeout=$true}
    } else { $result += @{role=$roles[$i];exit=$process.ExitCode;timeout=$false} }
}
$result | ConvertTo-Json
if (@($result | Where-Object { $_.timeout -or $_.exit -ne 0 }).Count) { exit 1 }

$evidence = Join-Path $revisionRoot 'Docs\Evidence\v0.8\network'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
foreach ($role in $roles) {
    $reportPath = Join-Path $revisionRoot "Builds\Windows\Chronicles\Saved\Tests\network-$role.json"
    if (-not (Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTime -lt $runStarted) { throw "Missing fresh multiplayer report for $role." }
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if (-not $report.passed -or -not $report.v05_features -or -not $report.v08_features) { throw "Multiplayer assertions failed for $role." }
    Copy-Item -LiteralPath $reportPath -Destination $evidence -Force
    Copy-Item -LiteralPath (Join-Path $logRoot "Network-v08-$role.log") -Destination $evidence -Force
}
for ($slot=0; $slot -lt 4; $slot++) {
    $detailPath = Join-Path $revisionRoot "Builds\Windows\Chronicles\Saved\Tests\network-v08-slot$slot.json"
    if (-not (Test-Path -LiteralPath $detailPath) -or (Get-Item -LiteralPath $detailPath).LastWriteTime -lt $runStarted) { throw "Missing fresh v0.8 detail report for slot $slot." }
    $detail = Get-Content -LiteralPath $detailPath -Raw | ConvertFrom-Json
    if (-not $detail.passed) { throw "v0.8 network assertions failed for slot $slot." }
    Copy-Item -LiteralPath $detailPath -Destination $evidence -Force
}
$audits = @(Get-ChildItem -LiteralPath (Join-Path $revisionRoot 'Builds\Windows\Chronicles\Saved\Tests') -Filter 'network-v07-*audit*.json' | Where-Object { $_.LastWriteTime -ge $runStarted })
if ($audits.Count -lt 1) { throw 'Missing fresh unsupported-gate command audit.' }
foreach ($audit in $audits) {
    $report = Get-Content -LiteralPath $audit.FullName -Raw | ConvertFrom-Json
    if (-not $report.passed -or -not $report.exactly_zero_cost -or -not $report.jobs_and_gate_unchanged) { throw 'Gate command charged or mutated state.' }
    Copy-Item -LiteralPath $audit.FullName -Destination $evidence -Force
}
Write-Output 'All four fresh multiplayer reports, v0.8 details, and exact gate-cost audits passed and were archived.'

