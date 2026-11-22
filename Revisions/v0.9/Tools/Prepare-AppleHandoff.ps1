[CmdletBinding()]
param(
    [switch]$CreateArchive,
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$revisionRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $revisionRoot 'Builds\Apple\Handoff' }
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)

$requiredPaths = @(
    'Game\Chronicles.uproject',
    'Game\Config\DefaultEngine.ini',
    'Game\Config\DefaultGame.ini',
    'Game\Config\DefaultGameUserSettings.ini',
    'Game\Config\DefaultInput.ini',
    'Game\Config\Mac\MacEngine.ini',
    'Game\Config\IOS\IOSEngine.ini',
    'Game\Source\Chronicles',
    'Game\Content\Data\catalog.json',
    'Game\Content\Data\visuals.json',
    'Game\Content\Data\names.json',
    'Game\Content\Data\Art',
    'Tools\Package-Mac.sh',
    'Tools\Package-iOS.sh',
    'Tools\Prepare-AppleHandoff.ps1',
    'Docs\Apple-v0.9.md'
)
foreach ($relative in $requiredPaths) {
    $path = Join-Path $revisionRoot $relative
    if (-not (Test-Path -LiteralPath $path)) { throw "Required Apple handoff input is missing: $relative" }
}

$uproject = Get-Content -LiteralPath (Join-Path $revisionRoot 'Game\Chronicles.uproject') -Raw | ConvertFrom-Json
if ([string]$uproject.EngineAssociation -ne '5.3') { throw 'This Apple source handoff expects Unreal Engine 5.3.' }
$defaultGame = Get-Content -LiteralPath (Join-Path $revisionRoot 'Game\Config\DefaultGame.ini') -Raw
if ($defaultGame -notmatch '(?m)^\+DirectoriesToAlwaysStageAsNonUFS\s*=\s*\(Path="Data"\)') {
    throw 'DefaultGame.ini must stage Content/Data loose for Mac/iOS parity with Windows and Android.'
}

$files = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
foreach ($relative in @(
    'Game\Chronicles.uproject',
    'Game\Config\DefaultEngine.ini',
    'Game\Config\DefaultGame.ini',
    'Game\Config\DefaultGameUserSettings.ini',
    'Game\Config\DefaultInput.ini',
    'Game\Config\Mac\MacEngine.ini',
    'Game\Config\IOS\IOSEngine.ini',
    'Tools\Package-Mac.sh',
    'Tools\Package-iOS.sh',
    'Tools\Prepare-AppleHandoff.ps1',
    'Docs\Apple-v0.9.md'
)) { $files.Add((Get-Item -LiteralPath (Join-Path $revisionRoot $relative))) }
$files.AddRange([System.IO.FileInfo[]](Get-ChildItem -LiteralPath (Join-Path $revisionRoot 'Game\Source') -File -Recurse))
$files.AddRange([System.IO.FileInfo[]](Get-ChildItem -LiteralPath (Join-Path $revisionRoot 'Game\Content') -File -Recurse))
$files = @($files | Sort-Object FullName -Unique)

$blockedExtensions = @('.p12', '.pfx', '.mobileprovision', '.pem', '.key', '.cer', '.crt', '.jks', '.keystore')
$blockedNames = @($files | Where-Object {
    ($blockedExtensions -contains $_.Extension.ToLowerInvariant()) -or
    ($_.Name -match '(?i)^\.env($|\.)|(^|[._-])(credential|private[-_]?key|provisioning[-_]?profile|signing[-_]?key)([._-]|$)')
})
if ($blockedNames.Count -gt 0) {
    $relativeBlocked = $blockedNames | ForEach-Object { $_.FullName.Substring($revisionRoot.Length).TrimStart('\') }
    throw "Sensitive-looking credential/profile file name is in an included source folder; remove it before handoff: $($relativeBlocked -join ', ')"
}
if ($files.Count -lt 10) { throw "Apple handoff source inventory is unexpectedly small ($($files.Count) files)." }

$byteTotal = ($files | Measure-Object -Property Length -Sum).Sum
Write-Host "Apple source handoff check passed: $($files.Count) source/config/content files, $([math]::Round($byteTotal / 1MB, 1)) MiB."
Write-Host 'Includes Game source, project config, all Game/Content (including Data, models, and icons), platform package scripts, and Apple build notes.'
Write-Host 'Excludes Unreal Engine binaries, Builds, Android toolchains, CookProject, Saved, Intermediate, Binaries, DerivedDataCache, and user signing credentials.'

if ($CreateArchive) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $archivePath = Join-Path $OutputDirectory 'Chronicles-v0.9-Apple-Source.zip'
    $sidecarPath = Join-Path $OutputDirectory 'Chronicles-v0.9-Apple-Source.manifest.json'
    if (Test-Path -LiteralPath $archivePath) { throw "Apple handoff archive already exists; choose another -OutputDirectory or move the previous archive first: $archivePath" }
    $staging = Join-Path ([System.IO.Path]::GetTempPath()) ("Chronicles-AppleHandoff-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $staging -Force | Out-Null
    try {
        $manifestFiles = foreach ($file in $files) {
            $relative = $file.FullName.Substring($revisionRoot.Length).TrimStart('\').Replace('\', '/')
            $target = Join-Path $staging ($relative.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            Copy-Item -LiteralPath $file.FullName -Destination $target
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
            [pscustomobject]@{ path = $relative; bytes = $file.Length; sha256 = $hash }
        }
        $manifest = [ordered]@{
            revision = '0.9'
            engineAssociation = '5.3'
            bundleIncludes = @('Game/Source', 'Game/Content', 'Game/Config', 'Tools/Package-Mac.sh', 'Tools/Package-iOS.sh')
            excluded = @('Unreal Engine licensed binaries', 'Builds', 'Tools/CookProject', 'Tools/Toolchain', 'Saved', 'Intermediate', 'Binaries', 'DerivedDataCache', 'Apple signing certificates and provisioning profiles')
            files = @($manifestFiles)
        }
        $json = $manifest | ConvertTo-Json -Depth 6
        $manifestStage = Join-Path $staging 'Apple-Source-Manifest.json'
        [System.IO.File]::WriteAllText($manifestStage, $json, [System.Text.UTF8Encoding]::new($false))
        [System.IO.File]::WriteAllText($sidecarPath, $json, [System.Text.UTF8Encoding]::new($false))
        Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $archivePath -CompressionLevel Optimal
        $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
        Write-Host "Apple source archive created: $archivePath"
        Write-Host "Archive SHA-256: $archiveHash"
        Write-Host "Manifest: $sidecarPath"
    } finally {
        Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue
    }
}
