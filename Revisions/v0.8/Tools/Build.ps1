[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE5\UE_5.3',
    [string]$Toolchain = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$project = Join-Path $root 'Game\Chronicles.uproject'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$logDir = Join-Path $root 'Game\Saved\BuildLogs'
$log = Join-Path $logDir 'Build-Win64-Development.log'
$errorLog = Join-Path $logDir 'Build-Win64-Development.stderr.log'
$autoSdkRoot = Join-Path $root 'Tools\Toolchain\AutoSDK'
$autoSdkProbe = Join-Path $autoSdkRoot 'HostWin64\Win64\Windows Kits\NETFXSDK\4.6.2\Include\um\mscoree.h'

if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Unreal project not found: $project" }
if (-not (Test-Path -LiteralPath $build -PathType Leaf)) { throw "Unreal Build.bat not found: $build" }
if (-not (Test-Path -LiteralPath $Toolchain -PathType Container)) { throw "Visual Studio Build Tools not found: $Toolchain" }
New-Item -ItemType Directory -Path $logDir -Force | Out-Null

if (Test-Path -LiteralPath $autoSdkProbe -PathType Leaf) {
    # UBT reads UE_SDKS_ROOT for AutoSDK discovery. This is process-scoped and does not alter machine settings.
    $env:UE_SDKS_ROOT = $autoSdkRoot
}

$arguments = @('Chronicles', 'Win64', 'Development', "-Project=`"$project`"", '-WaitMutex', '-NoHotReload', '-NoUBTMakefiles')
Write-Host "Building Chronicles Win64 Development. Log: $log"
$process = Start-Process -FilePath $build -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $build) -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput $log -RedirectStandardError $errorLog
if ($process.ExitCode -ne 0) { throw "Unreal build failed with exit code $($process.ExitCode). Review $log" }
Write-Host 'Build completed successfully.'
