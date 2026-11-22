$ErrorActionPreference = 'Stop'
$revisionRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$testExe = Join-Path $revisionRoot 'Builds\Windows\Chronicles\Binaries\Win64\Chronicles.exe'
$logRoot = Join-Path $revisionRoot 'Game\Saved\BuildLogs'
$port = 17889
if (Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue) { throw "Test port $port is occupied." }
$runStarted = Get-Date
$processes = @()
$roles = @('host','client1','client2','client3')
for ($i=0; $i -lt 4; $i++) {
    $role = $roles[$i]
    $url = if ($i -eq 0) { '/Engine/Maps/Entry?listen' } else { "127.0.0.1:$port" }
    $args = @($url,"-port=$port","-RTSNetTest=$role",'-RTSNetPlayers=4','-RTSNetStress','-RTSNetV04','-RTSNetV05','-RenderOffscreen','-d3d11','-unattended','-windowed','-ForceRes','-ResX=800','-ResY=600','-nosound','-ExecCmds="t.MaxFPS 30"',"-abslog=`"$logRoot\Network-v05-$role.log`"")
    $processes += Start-Process -WindowStyle Hidden -PassThru -FilePath $testExe -ArgumentList $args
    if ($i -eq 0) { Start-Sleep -Seconds 2 }
}
@($processes | ForEach-Object { @{id=$_.Id;path=$testExe} }) | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $logRoot 'Network-v05-processes.json')
Write-Output "Four offscreen v0.5 clients started on test port $port."
$deadline = (Get-Date).AddSeconds(100)
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

$evidence = Join-Path $revisionRoot 'Docs\Evidence\v0.5\network'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
foreach ($role in $roles) {
    $reportPath = Join-Path $revisionRoot "Builds\Windows\Chronicles\Saved\Tests\network-$role.json"
    if (-not (Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTime -lt $runStarted) { throw "Missing fresh multiplayer report for $role." }
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if (-not $report.passed -or -not $report.v05_features) { throw "Multiplayer assertions failed for $role." }
    Copy-Item -LiteralPath $reportPath -Destination $evidence -Force
    Copy-Item -LiteralPath (Join-Path $logRoot "Network-v05-$role.log") -Destination $evidence -Force
}
Write-Output 'All four fresh multiplayer reports passed and were archived.'
