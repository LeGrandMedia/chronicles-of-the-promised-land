[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$EvidenceName,
    [ValidateSet('d3d11', 'd3d12')][string]$Renderer = 'd3d12',
    [ValidateSet(100, 400)][int[]]$UnitCounts = @(100, 400),
    [switch]$Capture,
    [switch]$VerifyGeometry,
    [switch]$BatchUnitDeaths,
    [switch]$Lifecycle
)

# Recommended model: GPT-6.2 Sol; actual: root integration agent.
# Runs one graphical process at a time and preserves each report with its binary identity.
$ErrorActionPreference = 'Stop'
if (([int]$Capture.IsPresent + [int]$VerifyGeometry.IsPresent + [int]$Lifecycle.IsPresent) -gt 1) { throw 'Run captures, geometry and lifecycle verification separately.' }
if ($Lifecycle) {
    if ($PSBoundParameters.ContainsKey('UnitCounts') -and ($UnitCounts.Count -ne 1 -or $UnitCounts[0] -ne 400)) { throw 'Lifecycle verification requires only 400 units.' }
    $UnitCounts = @(400)
}
$root = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$exe = Join-Path $root 'Builds\Windows\Chronicles\Binaries\Win64\Chronicles.exe'
$saved = Join-Path $root 'Builds\Windows\Chronicles\Saved'
$evidence = Join-Path $root "Docs\Evidence\v0.9\performance\$EvidenceName"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Missing packaged game: $exe" }
$binaryHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
$sourceHashes = @{}
foreach ($name in @('RTSScene3D.cpp', 'RTSScene3D.h', 'RTSDeathBatches3D.cpp', 'RTSDeathStressTests.cpp', 'RTSDeathStressTests.h')) {
    $sourceHashes[$name] = (Get-FileHash -LiteralPath (Join-Path $root "Game\Source\Chronicles\$name") -Algorithm SHA256).Hash
}
foreach ($count in $UnitCounts) {
    $suffix = if ($Capture) { '-capture' } elseif ($VerifyGeometry) { '-geometry' } elseif ($Lifecycle) { '-lifecycle' } else { '' }
    $caseDir = Join-Path $evidence "$count$suffix"
    if (Test-Path -LiteralPath $caseDir) { throw "Refusing to overwrite evidence: $caseDir" }
    New-Item -ItemType Directory -Path $caseDir -Force | Out-Null
    $reportName = "death-stress-$count$suffix.json"
    $sourceReport = Join-Path $saved "Tests\$reportName"
    $oldReportHash = if (Test-Path -LiteralPath $sourceReport) { (Get-FileHash -LiteralPath $sourceReport).Hash } else { '' }
    $log = Join-Path $caseDir 'death-stress.log'
    $arguments = @('-RTSV09DeathStress', "-RTSDeathStressUnits=$count", "-$Renderer", '-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1600', '-ResY=900', '-unattended', '-nosound', "-abslog=`"$log`"")
    if ($Capture) { $arguments += '-RTSDeathStressCapture' }
    if ($VerifyGeometry) { $arguments += '-RTSDeathStressVerifyGeometry' }
    if ($BatchUnitDeaths) { $arguments += '-RTSBatchUnitDeaths' }
    if ($Lifecycle) { $arguments += '-RTSDeathStressLifecycle' }
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $root -WindowStyle Hidden -PassThru
    $exited = $process.WaitForExit(120000)
    if (-not $exited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    $finished = [DateTime]::UtcNow
    $run = [ordered]@{ started_utc = $started.ToString('o'); finished_utc = $finished.ToString('o'); executable = $exe; sha256 = $binaryHash; source_sha256 = $sourceHashes; arguments = $arguments; pid = $process.Id; timed_out = -not $exited; exit_code = $process.ExitCode }
    $run | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $caseDir 'run.json') -Encoding utf8
    if (Test-Path -LiteralPath $sourceReport) {
        $info = Get-Item -LiteralPath $sourceReport
        if ($info.LastWriteTimeUtc -ge $started -or (Get-FileHash -LiteralPath $sourceReport).Hash -ne $oldReportHash) {
            Copy-Item -LiteralPath $sourceReport -Destination (Join-Path $caseDir $reportName)
            $report = Get-Content -LiteralPath $sourceReport -Raw | ConvertFrom-Json
            foreach ($relative in $report.screenshots) {
                $capturePath = [IO.Path]::GetFullPath((Join-Path $saved $relative))
                if (-not $capturePath.StartsWith($saved + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Screenshot path escaped the packaged Saved directory.' }
                Copy-Item -LiteralPath $capturePath -Destination (Join-Path $caseDir ([IO.Path]::GetFileName($capturePath)))
            }
            if (-not $report.passed) { throw "Death stress $count failed: $($report.failure)" }
        } else { throw "No fresh report for $count; inspect $log" }
    } else { throw "No report for $count; inspect $log" }
    if (-not $exited -or $process.ExitCode -ne 0) { throw "Death stress $count failed: timeout=$(-not $exited), exit=$($process.ExitCode)" }
    Write-Host "Passed $count$suffix; evidence: $caseDir"
}
