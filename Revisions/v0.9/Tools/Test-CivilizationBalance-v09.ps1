[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$EvidenceName,
 [ValidateRange(0,14)][int[]]$Pairs=@(0,3,4,5,6,10),
 [ValidateRange(0,14)][int]$Map=0,
 [switch]$Legacy,
 [switch]$FirstPassRecruitment,
 [switch]$SecondPassRecruitment,
 [switch]$LegacyOpening,
 [switch]$RaidTrace,
 [switch]$LegacyRaidPolicy,
 [switch]$RaidExpedition,
 [switch]$GadiantonCombatFirst,
 [switch]$GadiantonFoodAssignment,
 [ValidateRange(1,12000)][int]$StepLimit=12000
)
# Recommended/actual: GPT-6.2 Sol / root integration agent. Headless only;
# no concurrent graphical fixtures, and no two jobs write the same report.
$ErrorActionPreference='Stop'
if(([int]$Legacy.IsPresent+[int]$FirstPassRecruitment.IsPresent+[int]$SecondPassRecruitment.IsPresent) -gt 1){throw 'Choose only one control arm'}
if($LegacyRaidPolicy -and $RaidExpedition){throw 'Choose only one raid policy'}
if($GadiantonCombatFirst -and $GadiantonFoodAssignment){throw 'Choose only one Gadianton opening policy'}
if(@($Pairs|Select-Object -Unique).Count -ne $Pairs.Count){throw 'Duplicate pairs'}
$root=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$exe=Join-Path $root 'Builds\Windows\Chronicles\Binaries\Win64\Chronicles.exe'
$saved=Join-Path $root 'Builds\Windows\Chronicles\Saved\Tests'
$hash=(Get-FileHash -LiteralPath $exe).Hash
for($offset=0;$offset -lt $Pairs.Count;$offset+=3){
 $jobs=@()
 foreach($pair in $Pairs[$offset..([Math]::Min($offset+2,$Pairs.Count-1))]){
  $dest=Join-Path $root "Docs\Evidence\v0.9\balance\$EvidenceName\map-$Map\pair-$pair"
  if(Test-Path -LiteralPath $dest){throw "Refusing to overwrite $dest"}
  New-Item -ItemType Directory -Path $dest -Force|Out-Null
  $log=Join-Path $dest 'balance.log'
  $args=@('-RTSV09BotBalance','-RTSV09BotSupportTrace','-RTSBalanceShards=15',"-RTSBalanceShard=$pair","-RTSBalanceMap=$Map","-RTSBalanceStepLimit=$StepLimit",'-nullrhi','-unattended','-nosound',"-abslog=`"$log`"")
  if($Legacy){$args+='-RTSV09BotLegacyCivilizationTuning'}
  if($FirstPassRecruitment){$args+='-RTSV09BotFirstPassRecruitment'}
  if($SecondPassRecruitment){$args+='-RTSV09BotSecondPassRecruitment'}
  if($LegacyOpening){$args+='-RTSV09BotLegacyOpening'}
  if($RaidTrace){$args+='-RTSV09BotRaidTrace'}
  if($LegacyRaidPolicy){$args+='-RTSV09BotLegacyRaidPolicy'}
  if($RaidExpedition){$args+='-RTSV09BotRaidExpedition'}
  if($GadiantonCombatFirst){$args+='-RTSV09BotGadiantonCombatFirst'}
  if($GadiantonFoodAssignment){$args+='-RTSV09BotGadiantonFoodAssignment'}
  $started=[DateTime]::UtcNow
  $proc=Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $root -WindowStyle Hidden -PassThru
  $jobs+=@{pair=$pair;dest=$dest;args=$args;started=$started;proc=$proc}
 }
 foreach($job in $jobs){
  $proc=$job.proc;$exited=$proc.WaitForExit(900000)
  if(-not $exited){Stop-Process -Id $proc.Id -Force;$proc.WaitForExit()}
  [ordered]@{started_utc=$job.started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');sha256=$hash;arguments=$job.args;exit_code=$proc.ExitCode;timed_out=(-not $exited)}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $job.dest 'run.json') -Encoding utf8
  $suffix=if($Map -eq 0){''}else{'-map-{0:D2}' -f $Map}
  $name='v09-bot-balance-{0:D2}-of-15{1}.json' -f $job.pair,$suffix
  $file=Join-Path $saved $name
  if(Test-Path -LiteralPath $file){if((Get-Item -LiteralPath $file).LastWriteTimeUtc -lt $job.started){throw "Stale report $file"};Copy-Item -LiteralPath $file -Destination $job.dest}
  else{throw "Missing report $file"}
  if(-not $exited -or $proc.ExitCode -ne 0){throw "Pair $($job.pair) failed"}
  $report=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json
  if(-not $report.complete -or -not $report.valid -or $report.map -ne $Map){throw "Invalid match report $file"}
  Write-Host "Passed pair $($job.pair), map $Map, legacy=$Legacy"
 }
}
