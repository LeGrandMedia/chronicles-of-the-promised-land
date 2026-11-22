[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE5\UE_5.3',
    [string]$AndroidToolchainRoot = 'D:\RTSToolchains\Android',
    [switch]$ReuseEngineCook
)

$ErrorActionPreference = 'Stop'
$revisionRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $revisionRoot 'Game\Chronicles.uproject'
$cookProject = Join-Path $PSScriptRoot 'CookProject\Chronicles.uproject'
$projectAndroidConfig = Join-Path $revisionRoot 'Game\Config\Android\AndroidEngine.ini'
$cookAndroidConfig = Join-Path $PSScriptRoot 'CookProject\Config\Android\AndroidEngine.ini'
$toolchainManifest = Join-Path $AndroidToolchainRoot 'toolchain.json'
$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$uat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$buildBatch = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$logDir = Join-Path $revisionRoot 'Game\Saved\BuildLogs'
$engineCookLog = Join-Path $logDir 'Cook-EngineContent-Android-ETC2.log'
$engineCookErrorLog = Join-Path $logDir 'Cook-EngineContent-Android-ETC2.stderr.log'
$packageLog = Join-Path $logDir 'Package-Android-arm64-ETC2-Development.log'
$packageErrorLog = Join-Path $logDir 'Package-Android-arm64-ETC2-Development.stderr.log'
$sourceCooked = Join-Path $PSScriptRoot 'CookProject\Saved\Cooked\Android_ETC2'
$targetCooked = Join-Path $revisionRoot 'Game\Saved\Cooked\Android_ETC2'
$archiveDir = Join-Path $revisionRoot 'Builds\Android'
$apkOutDir = Join-Path $revisionRoot 'Game\Binaries\Android'
$packageVersion = '0.9'
$expectedPackageName = 'com.chronicles.promisedland'
$expectedSdk = '33'
$expectedMinSdk = '26'

foreach ($requiredFile in @($project, $cookProject, $projectAndroidConfig, $cookAndroidConfig, $toolchainManifest, $editorCmd, $uat, $buildBatch)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) { throw "Required Android package input is missing: $requiredFile" }
}

$manifest = Get-Content -LiteralPath $toolchainManifest -Raw | ConvertFrom-Json
$sdkRoot = [System.IO.Path]::GetFullPath([string]$manifest.sdk)
$jdkRoot = [System.IO.Path]::GetFullPath([string]$manifest.jdk)
$ndkRoot = [System.IO.Path]::GetFullPath([string]$manifest.ndk)
$platformApi = ([string]$manifest.platform -replace '^android-', '')
if ($platformApi -ne $expectedSdk -or [string]$manifest.build_tools -ne '33.0.1' -or
    (Split-Path -Leaf $ndkRoot) -ne '25.1.8937393') {
    throw "Android toolchain does not match the pinned v0.9 profile (API 33, Build Tools 33.0.1, NDK 25.1.8937393): $toolchainManifest"
}

$requiredAndroidFiles = @(
    (Join-Path $sdkRoot 'platforms\android-33\android.jar'),
    (Join-Path $sdkRoot 'build-tools\33.0.1\aapt.exe'),
    (Join-Path $sdkRoot 'platform-tools\adb.exe'),
    (Join-Path $ndkRoot 'toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe'),
    (Join-Path $jdkRoot 'bin\java.exe')
)
foreach ($requiredFile in $requiredAndroidFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) { throw "Android SDK/JDK component missing: $requiredFile" }
}

$versionIni = Get-Content -LiteralPath (Join-Path $revisionRoot 'Game\Config\DefaultGame.ini') -Raw
if ($versionIni -notmatch '(?m)^ProjectVersion\s*=\s*0\.9\.0\s*$') { throw 'This packaging script requires project version 0.9.0.' }
if ($versionIni -notmatch '(?m)^\+DirectoriesToAlwaysStageAsNonUFS\s*=\s*\(Path="Data"\)') {
    throw 'Game/Config/DefaultGame.ini must stage Content/Data as loose non-UFS data (visuals, names, models, and artwork).'
}

$aapt = Join-Path $sdkRoot 'build-tools\33.0.1\aapt.exe'
if (-not (Test-Path -LiteralPath $aapt -PathType Leaf)) { throw "Android aapt is missing: $aapt" }

New-Item -ItemType Directory -Path $logDir,$archiveDir -Force | Out-Null
$autoSdkRoot = Join-Path $PSScriptRoot 'Toolchain\AutoSDK'
$autoSdkProbe = Join-Path $autoSdkRoot 'HostWin64\Win64\Windows Kits\NETFXSDK\4.6.2\Include\um\mscoree.h'
$scopedEnvironment = @('ANDROID_HOME','ANDROID_SDK_ROOT','ANDROID_NDK_ROOT','NDKROOT','JAVA_HOME','UE_SDKS_ROOT','uebp_LogFolder','uebp_FinalLogFolder','uebp_EngineSavedFolder')
$previousEnvironment = @{}
foreach ($name in $scopedEnvironment) { $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$runStarted = Get-Date

try {
    $env:ANDROID_HOME = $sdkRoot
    $env:ANDROID_SDK_ROOT = $sdkRoot
    $env:ANDROID_NDK_ROOT = $ndkRoot
    $env:NDKROOT = $ndkRoot
    $env:JAVA_HOME = $jdkRoot
    if (Test-Path -LiteralPath $autoSdkProbe -PathType Leaf) { $env:UE_SDKS_ROOT = $autoSdkRoot }
    $env:uebp_LogFolder = Join-Path $logDir 'Android-AutomationTool'
    $env:uebp_FinalLogFolder = $env:uebp_LogFolder
    $env:uebp_EngineSavedFolder = Join-Path $logDir 'Android-AutomationSaved'

    if ($ReuseEngineCook) {
        if (-not (Test-Path -LiteralPath $sourceCooked -PathType Container)) { throw "-ReuseEngineCook was requested, but cooked output is missing: $sourceCooked" }
        $cachedFiles = @(Get-ChildItem -LiteralPath (Join-Path $sourceCooked 'Engine') -File -Recurse -Filter '*.uasset')
        if ($cachedFiles.Count -lt 100) { throw "-ReuseEngineCook was requested, but cached Android engine content appears incomplete ($($cachedFiles.Count) .uasset files): $sourceCooked" }
        Write-Host "Reusing validated Android_ETC2 engine cook ($($cachedFiles.Count) engine packages): $sourceCooked"
    } else {
        Write-Host 'Cooking Unreal Engine content for Android_ETC2 from the content-only cooker.'
        $cookArguments = @(
            "`"$cookProject`"", '-run=Cook', '-TargetPlatform=Android_ETC2', '-Map=/Engine/Maps/Entry',
            '-Unversioned', '-UTF8Output', '-unattended', '-NullRHI', '-stdout', '-FullStdOutLogOutput'
        )
        $cook = Start-Process -FilePath $editorCmd -ArgumentList $cookArguments -WorkingDirectory (Split-Path -Parent $editorCmd) -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput $engineCookLog -RedirectStandardError $engineCookErrorLog
        if ($cook.ExitCode -ne 0) { throw "Android engine-content cook failed with exit code $($cook.ExitCode). Review $engineCookLog" }
        if (-not (Test-Path -LiteralPath $sourceCooked -PathType Container)) { throw "Android cook returned success but output is missing: $sourceCooked" }
    }

    New-Item -ItemType Directory -Path $targetCooked -Force | Out-Null
    & robocopy $sourceCooked $targetCooked /E /MT:8 /R:1 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copying Android cooked engine content failed: $LASTEXITCODE" }

    Write-Host 'Building and packaging the native arm64 APK (Development, API 33, ETC2).'
    $uatArguments = @(
        'BuildCookRun', "-project=`"$project`"", '-noP4', '-target=Chronicles', '-platform=Android',
        '-cookflavor=ETC2', '-clientarchitecture=arm64', '-clientconfig=Development', '-build', '-skipcook',
        '-nocompileeditor', '-nodebuginfo', '-stage', '-pak', '-package', '-archive',
        "-archivedirectory=`"$archiveDir`"", '-utf8output', '-unattended', '-stdout', '-FullStdOutLogOutput'
    )
    $package = Start-Process -FilePath $uat -ArgumentList $uatArguments -WorkingDirectory (Split-Path -Parent $uat) -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput $packageLog -RedirectStandardError $packageErrorLog
    if ($package.ExitCode -ne 0) { throw "Android arm64 package failed with exit code $($package.ExitCode). Review $packageLog" }

    $apkCandidates = @(Get-ChildItem -LiteralPath $archiveDir -Filter '*.apk' -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object LastWriteTime -ge $runStarted)
    if ($apkCandidates.Count -eq 0) {
        $apkCandidates = @(Get-ChildItem -LiteralPath $apkOutDir -Filter '*.apk' -File -ErrorAction SilentlyContinue |
            Where-Object LastWriteTime -ge $runStarted)
    }
    if ($apkCandidates.Count -ne 1) {
        throw "Expected exactly one new Android APK after UAT, found $($apkCandidates.Count). Review $packageLog and $archiveDir."
    }

    $apk = $apkCandidates[0]
    $badging = & $aapt dump badging $apk.FullName 2>&1
    if ($LASTEXITCODE -ne 0) { throw "aapt could not inspect the new APK: $($apk.FullName)" }
    $badgingText = $badging -join "`n"
    if ($badgingText -notmatch "(?m)^package: name='$([regex]::Escape($expectedPackageName))'") { throw 'APK package identifier does not match the v0.9 Android profile.' }
    if ($badgingText -notmatch "(?m)^sdkVersion:'$expectedMinSdk'") { throw "APK minimum SDK is not $expectedMinSdk." }
    if ($badgingText -notmatch "(?m)^targetSdkVersion:'$expectedSdk'") { throw "APK target SDK is not $expectedSdk." }
    if ($badgingText -notmatch "(?m)^native-code:.*'arm64-v8a'") { throw 'APK does not contain the arm64-v8a native architecture.' }
    $obbCandidates = @(
        Get-ChildItem -LiteralPath $archiveDir -Filter '*.obb' -File -Recurse -ErrorAction SilentlyContinue | Where-Object LastWriteTime -ge $runStarted
        Get-ChildItem -LiteralPath $apkOutDir -Filter '*.obb' -File -ErrorAction SilentlyContinue | Where-Object LastWriteTime -ge $runStarted
    )
    if ($obbCandidates.Count -gt 0) { throw "An OBB was produced; the Android APK is not self-contained: $($obbCandidates[0].FullName)" }

    $finalApk = Join-Path $archiveDir 'Chronicles-Android-arm64-ETC2-v0.9.apk'
    Copy-Item -LiteralPath $apk.FullName -Destination $finalApk -Force
    $hash = (Get-FileHash -LiteralPath $finalApk -Algorithm SHA256).Hash
    Write-Host "Android APK created: $finalApk"
    Write-Host "Size: $((Get-Item -LiteralPath $finalApk).Length) bytes; SHA-256: $hash"
    Write-Host 'This is a sideload Development build targeting API 33; it is not a Google Play release artifact.'
} finally {
    foreach ($name in $scopedEnvironment) {
        [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process')
    }
}

