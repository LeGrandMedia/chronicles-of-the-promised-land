[CmdletBinding()]
param(
    [string]$PackageDir = (Join-Path $PSScriptRoot '..\Builds\Windows'),
    [string]$OutputDir = (Join-Path $PSScriptRoot '..\Builds\Installer'),
    [string]$CompilerPath
)

$ErrorActionPreference = 'Stop'
$revisionRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$packagePath = [System.IO.Path]::GetFullPath($PackageDir)
$outputPath = [System.IO.Path]::GetFullPath($OutputDir)
$expectedPackagePath = [System.IO.Path]::GetFullPath((Join-Path $revisionRoot 'Builds\Windows'))
$projectIni = Join-Path $revisionRoot 'Game\Config\DefaultGame.ini'
$installerScript = Join-Path $revisionRoot 'Installer\Chronicles.iss'
$installerReadme = Join-Path $revisionRoot 'Installer\README.txt'

if (-not [string]::Equals($packagePath.TrimEnd('\'), $expectedPackagePath.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) {
    throw "PackageDir must be this revision's staged package: $expectedPackagePath"
}
if (-not (Test-Path -LiteralPath $projectIni -PathType Leaf)) { throw "Project version source not found: $projectIni" }
if (-not (Test-Path -LiteralPath $installerScript -PathType Leaf)) { throw "Inno Setup script not found: $installerScript" }
if (-not (Test-Path -LiteralPath $installerReadme -PathType Leaf)) { throw "Installer README not found: $installerReadme" }
if (-not (Test-Path -LiteralPath $packagePath -PathType Container)) {
    throw "No v0.9 Windows package found at $packagePath. Build it first with Tools/Package.ps1; this script does not build or cook the game."
}

$gameExe = Join-Path $packagePath 'Chronicles.exe'
$prereqExe = Join-Path $packagePath 'Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe'
if (-not (Test-Path -LiteralPath $gameExe -PathType Leaf)) { throw "Packaged game executable missing: $gameExe" }
if (-not (Test-Path -LiteralPath $prereqExe -PathType Leaf)) { throw "Epic runtime prerequisite installer missing: $prereqExe. Ensure UAT packaging includes prerequisites." }

$versionText = Get-Content -LiteralPath $projectIni -Raw
if ($versionText -notmatch '(?m)^ProjectVersion\s*=\s*([0-9]+\.[0-9]+\.[0-9]+)\s*$') { throw "Could not read numeric ProjectVersion from $projectIni" }
$appVersion = $Matches[1]
if ($appVersion -ne '0.9.0') { throw "This v0.9 installer source only accepts project version 0.9.0; found $appVersion" }

if (-not $CompilerPath) {
    $compilerCandidates = @(
        (Join-Path $PSScriptRoot 'Toolchain\InnoSetup7\ISCC.exe'),
        'C:\Program Files (x86)\Inno Setup 7\ISCC.exe',
        'C:\Program Files\Inno Setup 7\ISCC.exe',
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
        'C:\Program Files\Inno Setup 6\ISCC.exe'
    )
    $CompilerPath = $compilerCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
if (-not $CompilerPath -or -not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) {
    throw 'Inno Setup ISCC.exe not found. Install Inno Setup 7 or pass -CompilerPath. See Docs/Installer-v0.9.md.'
}

# Fail early if debug symbols or development caches have accidentally entered the stage.
$forbiddenFiles = @(Get-ChildItem -LiteralPath $packagePath -Recurse -File -Force | Where-Object {
    $_.Extension -in @('.pdb', '.obj', '.ilk', '.exp', '.lib', '.pch', '.idb', '.ipch', '.sdf', '.suo', '.user')
})
if ($forbiddenFiles.Count -gt 0) {
    $examples = ($forbiddenFiles | Select-Object -First 5 -ExpandProperty FullName) -join [Environment]::NewLine
    throw "Staged package contains development/debug output that must be removed before installer build:`n$examples"
}

New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
$previousPackageEnv = $env:CHRONICLES_PACKAGE_DIR
$previousOutputEnv = $env:CHRONICLES_INSTALLER_OUTPUT
$previousVersionEnv = $env:CHRONICLES_APP_VERSION
$env:CHRONICLES_PACKAGE_DIR = $packagePath
$env:CHRONICLES_INSTALLER_OUTPUT = $outputPath
$env:CHRONICLES_APP_VERSION = $appVersion
Write-Host "Compiling the v$appVersion Windows x64 installer from $packagePath"
Write-Host "Installer output: $outputPath"
try {
    & $CompilerPath $installerScript
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed with exit code $LASTEXITCODE" }
} finally {
    if ($null -eq $previousPackageEnv) { Remove-Item Env:CHRONICLES_PACKAGE_DIR -ErrorAction SilentlyContinue } else { $env:CHRONICLES_PACKAGE_DIR = $previousPackageEnv }
    if ($null -eq $previousOutputEnv) { Remove-Item Env:CHRONICLES_INSTALLER_OUTPUT -ErrorAction SilentlyContinue } else { $env:CHRONICLES_INSTALLER_OUTPUT = $previousOutputEnv }
    if ($null -eq $previousVersionEnv) { Remove-Item Env:CHRONICLES_APP_VERSION -ErrorAction SilentlyContinue } else { $env:CHRONICLES_APP_VERSION = $previousVersionEnv }
}
$installer = Join-Path $outputPath "Chronicles-Setup-v$appVersion-Windows-x64.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { throw "Inno Setup reported success but output is missing: $installer" }
Write-Host "Installer created: $installer"
