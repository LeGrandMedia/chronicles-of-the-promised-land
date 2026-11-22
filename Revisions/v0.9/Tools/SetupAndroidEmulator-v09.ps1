[CmdletBinding()]
param(
    [string]$AndroidToolchainRoot = 'D:\RTSToolchains\Android',
    [string]$VmRoot = 'D:\RTSTestVM\v0.9\Android',
    [string]$AvdName = 'Chronicles_v09_ARM64_TCG',
    [string]$ImagePackage = 'system-images;android-33;google_apis;arm64-v8a',
    [string]$DeviceProfile = 'pixel_2'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$revisionRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$manifestPath = Join-Path $AndroidToolchainRoot 'toolchain.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Android toolchain manifest missing: $manifestPath" }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$sdkRoot = [IO.Path]::GetFullPath([string]$manifest.sdk)
$jdkRoot = [IO.Path]::GetFullPath([string]$manifest.jdk)
$sdkManager = Join-Path $sdkRoot 'cmdline-tools\9.0\bin\sdkmanager.bat'
$avdManager = Join-Path $sdkRoot 'cmdline-tools\9.0\bin\avdmanager.bat'
$javaExe = Join-Path $jdkRoot 'bin\java.exe'
$avdHome = Join-Path $VmRoot 'avd'
$androidUserHome = Join-Path $VmRoot 'user-home'
$avdIni = Join-Path $avdHome "$AvdName.ini"
$avdDir = Join-Path $avdHome "$AvdName.avd"
$logPath = Join-Path $VmRoot 'SetupAndroidEmulator-v09.log'

foreach ($path in @($sdkManager, $avdManager, $javaExe)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required Android tool is missing: $path" }
}
if ((Test-Path -LiteralPath $avdIni) -or (Test-Path -LiteralPath $avdDir)) {
    throw "Refusing to replace existing task-scoped AVD '$AvdName'. Preserve it or select a different name."
}
New-Item -ItemType Directory -Path $avdHome,$androidUserHome,$VmRoot -Force | Out-Null

$environmentNames = @('ANDROID_HOME','ANDROID_SDK_ROOT','ANDROID_AVD_HOME','ANDROID_USER_HOME','JAVA_HOME','PATH')
$previousEnvironment = @{}
foreach ($name in $environmentNames) { $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$logLines = [System.Collections.Generic.List[string]]::new()
try {
    $env:ANDROID_HOME = $sdkRoot
    $env:ANDROID_SDK_ROOT = $sdkRoot
    $env:ANDROID_AVD_HOME = $avdHome
    $env:ANDROID_USER_HOME = $androidUserHome
    $env:JAVA_HOME = $jdkRoot
    $env:PATH = (Join-Path $jdkRoot 'bin') + ';' + $previousEnvironment['PATH']

    $packages = @('emulator', $ImagePackage)
    Write-Host "Installing official Android SDK packages into ${sdkRoot}: $($packages -join ', ')"
    $installOutput = & $sdkManager "--sdk_root=$sdkRoot" @packages 2>&1
    $installCode = $LASTEXITCODE
    $installOutput | ForEach-Object { $logLines.Add([string]$_) }
    if ($installCode -ne 0) {
        throw "sdkmanager failed with exit code $installCode. If it requested a license not already accepted, review and accept it through the normal SDK tools before retrying."
    }

    $emulatorExe = Join-Path $sdkRoot 'emulator\emulator.exe'
    if (-not (Test-Path -LiteralPath $emulatorExe -PathType Leaf)) { throw "sdkmanager reported success but emulator.exe is missing: $emulatorExe" }
    if (-not (Test-Path -LiteralPath (Join-Path $sdkRoot 'system-images\android-33\google_apis\arm64-v8a\system.img') -PathType Leaf)) {
        throw "The requested API 33 ARM64 image is missing under $sdkRoot"
    }

    Write-Host "Creating isolated AVD $AvdName at $avdHome"
    $createOutput = & $avdManager create avd --name $AvdName --package $ImagePackage --device $DeviceProfile 2>&1
    $createCode = $LASTEXITCODE
    $createOutput | ForEach-Object { $logLines.Add([string]$_) }
    if ($createCode -ne 0) { throw "avdmanager failed with exit code $createCode" }
    if (-not (Test-Path -LiteralPath $avdIni -PathType Leaf) -or -not (Test-Path -LiteralPath $avdDir -PathType Container)) {
        throw "avdmanager returned success but the scoped AVD files were not created under $avdHome"
    }

    $configPath = Join-Path $avdDir 'config.ini'
    $overrides = [ordered]@{
        'hw.gpu.enabled' = 'yes'
        'hw.gpu.mode' = 'swiftshader'
        'hw.ramSize' = '6144M'
        'hw.cpu.ncore' = '2'
        'hw.audioInput' = 'no'
        'hw.camera.back' = 'none'
        'hw.camera.front' = 'none'
        'hw.lcd.width' = '720'
        'hw.lcd.height' = '1280'
        'hw.lcd.density' = '320'
        'disk.dataPartition.size' = '8G'
        'showDeviceFrame' = 'no'
    }
    $keysToReplace = @($overrides.Keys)
    $configLines = @(Get-Content -LiteralPath $configPath | Where-Object {
        $line = [string]$_
        $separator = $line.IndexOf('=')
        $separator -lt 0 -or $line.Substring(0, $separator) -notin $keysToReplace
    })
    $configLines += @($overrides.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" })
    Set-Content -LiteralPath $configPath -Value $configLines -Encoding ascii
    $logLines.Add("AVD created: $AvdName")
    $logLines.Add("Image: $ImagePackage")
    $logLines.Add("Device profile: $DeviceProfile")
    $logLines.Add("AVD path: $avdDir")
    $logLines.Add('GPU mode: SwiftShader software rendering')
    $logLines.Add('VM state: created only; emulator was not launched')
    $logLines | Set-Content -LiteralPath $logPath -Encoding utf8
    Write-Host "Prepared ARM64 API 33 AVD: $AvdName"
    Write-Host "AVD home: $avdHome"
    Write-Host "Setup log: $logPath"
    Write-Host 'No emulator startup or APK install was performed.'
} catch {
    $logLines.Add("ERROR: $($_.Exception.Message)")
    $logLines | Set-Content -LiteralPath $logPath -Encoding utf8
    throw
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process')
    }
}
