$ErrorActionPreference = 'Stop'
$revisionRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$testExe = Join-Path $revisionRoot 'Builds\Windows\Chronicles\Binaries\Win64\Chronicles.exe'
$logRoot = Join-Path $revisionRoot 'Game\Saved\BuildLogs'
$port = 17889
if (Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue) { throw "Test port $port is occupied." }
$processes = @()
$roles = @('host','client1','client2','client3')
for ($i=0; $i -lt 4; $i++) {
    $role = $roles[$i]
    $url = if ($i -eq 0) { '/Engine/Maps/Entry?listen' } else { "127.0.0.1:$port" }
    $args = @($url,"-port=$port","-RTSNetTest=$role",'-RTSNetPlayers=4','-RTSNetStress','-RTSNetV04','-RenderOffscreen','-d3d11','-unattended','-windowed','-ForceRes','-ResX=800','-ResY=600','-nosound','-ExecCmds="t.MaxFPS 30"',"-abslog=`"$logRoot\Network-v04-$role.log`"")
    $processes += Start-Process -WindowStyle Hidden -PassThru -FilePath $testExe -ArgumentList $args
    if ($i -eq 0) { Start-Sleep -Seconds 2 }
}
@($processes | ForEach-Object { @{id=$_.Id;path=$testExe} }) | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $logRoot 'Network-v04-processes.json')
Write-Output "Four offscreen v0.4 clients started on test port $port."
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
