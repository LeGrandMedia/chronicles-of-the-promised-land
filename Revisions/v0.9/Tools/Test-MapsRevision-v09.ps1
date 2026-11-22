[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$EvidenceName,
 [ValidateSet('maps','native','scene','input','captures','bots','gates','touch','visibility')][string[]]$Suites=@('maps','native','scene','input','captures'),
 [ValidateSet('1600x900','1280x720','888x500')][string]$Size='1600x900'
)
# Recommended/actual: GPT-6.2 Sol / root integration agent.
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$exe=Join-Path $root 'Builds\Windows\Chronicles\Binaries\Win64\Chronicles.exe'
$saved=Join-Path $root 'Builds\Windows\Chronicles\Saved'
$hash=(Get-FileHash -LiteralPath $exe).Hash
$flags=@{maps='RTSV09MapTest';native='RTSSelfTest';scene='RTSV09SceneTest';input='RTSInputTest';captures='RTSV09MapCapture';bots='RTSV09BotTest';gates='RTSV09GateTraversalTest';touch='RTSV09TouchTest';visibility='RTSV09EntityVisibilityTest'}
$reports=@{maps='v09-maps.json';scene='scene3d-tests.json';input='input-tests.json';captures='v09-map-captures.json';bots='v09-bot-tests.json';gates='gate-traversal-tests.json';touch='v09-touch-tests.json';visibility='v09-entity-visibility.json'}
$nativeReports=@('combat-v05-tests.json','core-v04-tests.json','core-v07-tests.json','customization-tests.json','economy-v03-tests.json','economy-v04-tests.json','economy-v05-tests.json','rally-tests.json','religion-v03-tests.json','religion-v07-tests.json','religion-v08-tests.json','scenario-tests.json','self-tests.json','v07-economy-tests.json','v07-selection-tests.json','v08-economy-tests.json','v08-presentation-tests.json','visual-tests.json','visual-v07-animation-tests.json')
foreach($suite in $Suites){
 $dest=Join-Path $root "Docs\Evidence\v0.9\maps\$EvidenceName\$suite-$Size"
 if(Test-Path -LiteralPath $dest){throw "Refusing to overwrite $dest"}
 New-Item -ItemType Directory -Path $dest -Force | Out-Null
 $started=[DateTime]::UtcNow;$log=Join-Path $dest 'test.log';$dims=$Size.Split('x')
 $args=@("-$($flags[$suite])",'-unattended','-nosound',"-abslog=`"$log`"")
 if($suite -eq 'touch'){$args+='-RTSTouchPreview'}
 if($suite -in @('maps','native','bots','gates','visibility')){$args+='-nullrhi'}else{$args+=@('-d3d11','-RenderOffscreen','-windowed','-ForceRes',"-ResX=$($dims[0])","-ResY=$($dims[1])")}
 $proc=Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $root -PassThru -WindowStyle Hidden
 $exited=$proc.WaitForExit(240000)
 if(-not $exited){Stop-Process -Id $proc.Id -Force;$proc.WaitForExit()}
 [ordered]@{started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');sha256=$hash;arguments=$args;exit_code=$proc.ExitCode;timed_out=(-not $exited)}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $dest 'run.json') -Encoding utf8
 $fresh=@(Get-ChildItem -LiteralPath (Join-Path $saved 'Tests') -Filter '*.json'|Where-Object {$_.LastWriteTimeUtc -ge $started})
 if($suite -eq 'gates'){$fresh=@(Get-ChildItem -LiteralPath (Join-Path $saved 'GateTraversalTest') -Filter '*.json'|Where-Object {$_.LastWriteTimeUtc -ge $started})}
 # Other headless probes may be running concurrently. Archive only this suite.
 if($suite -eq 'native'){$fresh=@($fresh|Where-Object {$_.Name -in $nativeReports})}
 else{$fresh=@($fresh|Where-Object {$_.Name -eq $reports[$suite]})}
 foreach($item in $fresh){Copy-Item -LiteralPath $item.FullName -Destination $dest}
 if($suite -in @('scene','input','captures','touch')){Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter '*.png'|Where-Object {$_.LastWriteTimeUtc -ge $started}|Copy-Item -Destination $dest}
 if(-not $exited -or $proc.ExitCode -ne 0){throw "$suite failed (exit $($proc.ExitCode)); see $dest"}
 if($suite -eq 'native'){if($fresh.Count -ne $nativeReports.Count){throw 'Native suite missing reports'}}
 else{if(-not (Test-Path -LiteralPath (Join-Path $dest $reports[$suite]))){throw "$suite missing fresh report"}}
 foreach($item in $fresh){$report=Get-Content -LiteralPath $item.FullName -Raw|ConvertFrom-Json;if($report.failed -gt 0 -or ($report.passed -is [bool] -and -not $report.passed)){throw "Failed assertions: $($item.Name)"}}
 Write-Host "Passed $suite ($Size): $dest"
}
