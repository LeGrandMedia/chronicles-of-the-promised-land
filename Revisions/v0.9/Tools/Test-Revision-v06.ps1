$ErrorActionPreference = 'Stop'
$revisionRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$testExe = Join-Path $revisionRoot 'Builds\Windows\Chronicles\Binaries\Win64\Chronicles.exe'
$saved = Join-Path $revisionRoot 'Builds\Windows\Chronicles\Saved'
$evidence = Join-Path $revisionRoot 'Docs\Evidence\v0.6'
$logRoot = Join-Path $revisionRoot 'Game\Saved\BuildLogs'
foreach ($size in @('native','1600x900','1280x720')) {
    $native = $size -eq 'native'
    $dims = if ($native) { @('1600','900') } else { $size.Split('x') }
    $flag = if ($native) { '-RTSSelfTest' } else { '-RTSInputTest' }
    $log = Join-Path $logRoot "Test-v06-$size.log"
    $args = @($flag,'-RenderOffscreen','-d3d11','-unattended','-windowed','-ForceRes',"-ResX=$($dims[0])","-ResY=$($dims[1])",'-nosound',"-abslog=`"$log`"")
    $testProcess = Start-Process -WindowStyle Hidden -PassThru -FilePath $testExe -ArgumentList $args
    if (-not $testProcess.WaitForExit(90000)) {
        $current = Get-Process -Id $testProcess.Id -ErrorAction SilentlyContinue
        if ($current -and $current.Path -eq $testExe) { Stop-Process -Id $current.Id }
        throw "v0.6 $size test timed out."
    }
    if ($testProcess.ExitCode -ne 0) { throw "v0.6 $size test failed. See $log" }
    if ($native) {
        foreach ($reportFile in Get-ChildItem -LiteralPath (Join-Path $saved 'Tests') -Filter '*tests.json' | Where-Object { $_.Name -ne 'input-tests.json' }) {
            $report = Get-Content -LiteralPath $reportFile.FullName -Raw | ConvertFrom-Json
            if ($report.failed -gt 0 -or ($report.passed -is [bool] -and -not $report.passed)) { throw "Failed native assertions in $($reportFile.Name)." }
        }
    } else {
        $report = Get-Content -LiteralPath (Join-Path $saved 'Tests\input-tests.json') -Raw | ConvertFrom-Json
        if (-not $report.passed) { throw "v0.6 $size viewport assertions failed; see input-tests.json." }
    }
    $dest = Join-Path $evidence $size
    New-Item -ItemType Directory -Path $dest -Force | Out-Null
    if ($native) { Get-ChildItem -LiteralPath (Join-Path $saved 'Tests') -Filter '*tests.json' | Where-Object { $_.Name -ne 'input-tests.json' } | Copy-Item -Destination $dest -Force }
    else {
        Copy-Item -LiteralPath (Join-Path $saved 'Tests\input-tests.json') -Destination $dest -Force
        Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'input-*.png' | Copy-Item -Destination $dest -Force
    }
    Copy-Item -LiteralPath $log -Destination $dest -Force
    Write-Output "Passed and archived v0.6 $size checks."
}
