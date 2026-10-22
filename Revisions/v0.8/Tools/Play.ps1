[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE5\UE_5.3'
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$gameDir = Join-Path $root 'Game'
$project = Join-Path $gameDir 'Chronicles.uproject'
$packaged = Join-Path $root 'Builds\Windows\Chronicles.exe'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'

if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Unreal project not found: $project" }
if (Test-Path -LiteralPath $packaged -PathType Leaf) {
    $exe = $packaged
    $arguments = @()
    Write-Host "Launching packaged Chronicles from $exe"
} else {
    if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Neither the packaged game nor Unreal Editor was found. Expected: $packaged or $editor" }
    $exe = $editor
    $arguments = @('-game', "`"$project`"")
    Write-Host "No packaged game found. Launching the project in Unreal Editor standalone game mode. Run Tools\Package.ps1 to create the portable build."
}

# Keep the game window visible so the player can interact with it.
if ($arguments.Count -gt 0) {
    Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $exe)
} else {
    Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe)
}
