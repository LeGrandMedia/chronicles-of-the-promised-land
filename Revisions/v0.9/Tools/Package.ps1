[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE5\UE_5.3',
    [switch]$ReuseEngineCook
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$project = Join-Path $root 'Game\Chronicles.uproject'
$cookProject = Join-Path $root 'Tools\CookProject\Chronicles.uproject'
$buildScript = Join-Path $PSScriptRoot 'Build.ps1'
$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$uat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$logDir = Join-Path $root 'Game\Saved\BuildLogs'
$log = Join-Path $logDir 'Package-Win64-Development.log'
$cookLog = Join-Path $logDir 'Cook-EngineContent-Windows.log'
$cookErrorLog = Join-Path $logDir 'Cook-EngineContent-Windows.stderr.log'
$archive = Join-Path $root 'Builds'
$packaged = Join-Path $archive 'Windows\Chronicles.exe'
$errorLog = Join-Path $logDir 'Package-Win64-Development.stderr.log'
$sourceCooked = Join-Path $root 'Tools\CookProject\Saved\Cooked\Windows'
$targetCooked = Join-Path $root 'Game\Saved\Cooked\Windows'

if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Unreal project not found: $project" }
if (-not (Test-Path -LiteralPath $cookProject -PathType Leaf)) { throw "Content-only cook project not found: $cookProject" }
if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) { throw "Build script not found: $buildScript" }
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) { throw "UnrealEditor-Cmd.exe not found: $editorCmd" }
if (-not (Test-Path -LiteralPath $uat -PathType Leaf)) { throw "RunUAT.bat not found: $uat" }
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
New-Item -ItemType Directory -Path $archive -Force | Out-Null

$sdkRoot = Join-Path $root 'Tools\Toolchain\AutoSDK'
$sdkProbe = Join-Path $sdkRoot 'HostWin64\Win64\Windows Kits\NETFXSDK\4.6.2\Include\um\mscoree.h'
if (Test-Path -LiteralPath $sdkProbe -PathType Leaf) { $env:UE_SDKS_ROOT = $sdkRoot }

Write-Host 'Building the Chronicles game target.'
& $buildScript -EngineRoot $EngineRoot

$cookArguments = @("`"$cookProject`"", '-run=Cook', '-TargetPlatform=Windows', '-Map=/Engine/Maps/Entry', '-Unversioned', '-UTF8Output', '-unattended', '-NullRHI', '-stdout', '-FullStdOutLogOutput')
if (-not $ReuseEngineCook) {
    Write-Host "Cooking engine content for Windows. Log: $cookLog"
    $cookProcess = Start-Process -FilePath $editorCmd -ArgumentList $cookArguments -WorkingDirectory (Split-Path -Parent $editorCmd) -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput $cookLog -RedirectStandardError $cookErrorLog
    if ($cookProcess.ExitCode -ne 0) { throw "Engine-content cook failed with exit code $($cookProcess.ExitCode). Review $cookLog" }
} else { Write-Host 'Reusing the unchanged UE 5.3 Entry map and engine-content cook.' }
if (-not (Test-Path -LiteralPath $sourceCooked -PathType Container)) { throw "Cook command succeeded but its cooked output is missing: $sourceCooked" }
New-Item -ItemType Directory -Path $targetCooked -Force | Out-Null
& robocopy $sourceCooked $targetCooked /E /MT:8 /R:1 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "Cooked content copy failed: $LASTEXITCODE" }

# Keep AutomationTool's disposable logs inside this revision.
$env:uebp_LogFolder = Join-Path $logDir 'AutomationTool'
$env:uebp_FinalLogFolder = $env:uebp_LogFolder
$env:uebp_EngineSavedFolder = Join-Path $logDir 'AutomationSaved'

$arguments = @('BuildCookRun', "-project=`"$project`"", '-noP4', '-platform=Win64', '-clientconfig=Development', '-skipbuild', '-skipcook', '-nocompileeditor', '-nodebuginfo', '-prereqs', '-stage', '-pak', '-archive', "-archivedirectory=`"$archive`"")
Write-Host "Staging and packaging Chronicles for Win64. Log: $log"
$process = Start-Process -FilePath $uat -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $uat) -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput $log -RedirectStandardError $errorLog
if ($process.ExitCode -ne 0) { throw "Unreal packaging failed with exit code $($process.ExitCode). Review $log" }
if (-not (Test-Path -LiteralPath $packaged -PathType Leaf)) { throw "Package command returned success, but expected game executable is missing: $packaged. Review $log" }
Write-Host "Package completed successfully: $packaged"
