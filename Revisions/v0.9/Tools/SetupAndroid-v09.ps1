[CmdletBinding()]
param([string]$ToolchainRoot = 'D:\RTSToolchains\Android')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
New-Item -ItemType Directory -Force -Path $ToolchainRoot | Out-Null
$downloadRoot = Join-Path $ToolchainRoot 'downloads'
$sdkRoot = Join-Path $ToolchainRoot 'sdk'
New-Item -ItemType Directory -Force -Path $downloadRoot,$sdkRoot | Out-Null
function Get-VerifiedArchive([string]$Url,[string]$Destination,[string]$Hash,[string]$Algorithm) {
    if (Test-Path -LiteralPath $Destination) {
        if ((Get-FileHash -LiteralPath $Destination -Algorithm $Algorithm).Hash -eq $Hash) { return }
        throw "An existing archive has an unexpected checksum: $Destination"
    }
    Invoke-WebRequest -Uri $Url -OutFile $Destination
    if ((Get-FileHash -LiteralPath $Destination -Algorithm $Algorithm).Hash -ne $Hash) { throw "Vendor checksum verification failed: $Destination" }
}
# Google publishes this version in its SDK repository. Version 9 supports the
# JDK 17 required by Unreal 5.3, without changing any system environment variables.
Write-Output 'Resolving official Google SDK and Eclipse Temurin JDK packages.'
$repository = [xml](Invoke-WebRequest -Uri 'https://dl.google.com/android/repository/repository2-1.xml').Content
$package = $repository.SelectNodes('//*[local-name()="remotePackage"]') | Where-Object { $_.path -eq 'cmdline-tools;9.0' }
$archive = ($package.archives.archive | Where-Object { $_.'host-os' -eq 'windows' }).complete
if (-not $archive.url) { throw 'Google no longer lists command-line tools 9.0.' }
$sdkZip = Join-Path $downloadRoot 'google-commandline-tools-9.zip'
Get-VerifiedArchive ('https://dl.google.com/android/repository/' + $archive.url) $sdkZip $archive.checksum 'SHA1'
$javaRelease = Invoke-RestMethod -Uri 'https://api.adoptium.net/v3/assets/latest/17/hotspot?architecture=x64&image_type=jdk&os=windows&vendor=eclipse'
$javaPackage = $javaRelease[0].binary.package
$javaZip = Join-Path $downloadRoot $javaPackage.name
Get-VerifiedArchive $javaPackage.link $javaZip $javaPackage.checksum 'SHA256'
$commandRoot = Join-Path $sdkRoot 'cmdline-tools\9.0'
if (-not (Test-Path -LiteralPath (Join-Path $commandRoot 'bin\sdkmanager.bat'))) {
    $extractRoot = Join-Path $ToolchainRoot 'google-extract'
    Expand-Archive -LiteralPath $sdkZip -DestinationPath $extractRoot -Force
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $commandRoot) | Out-Null
    Copy-Item -LiteralPath (Join-Path $extractRoot 'cmdline-tools') -Destination $commandRoot -Recurse
}
$javaRoot = Join-Path $ToolchainRoot 'jdk'
if (-not (Test-Path -LiteralPath $javaRoot)) { Expand-Archive -LiteralPath $javaZip -DestinationPath $javaRoot }
$javaInstall = Get-ChildItem -LiteralPath $javaRoot -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'bin\java.exe') } | Select-Object -First 1
if (-not $javaInstall) { throw 'Temurin extraction did not provide java.exe.' }
$env:JAVA_HOME=$javaInstall.FullName
$env:ANDROID_HOME=$sdkRoot
$env:ANDROID_SDK_ROOT=$sdkRoot
$env:NDKROOT=Join-Path $sdkRoot 'ndk\25.1.8937393'
$manager = Join-Path $commandRoot 'bin\sdkmanager.bat'
Write-Output 'Installing UE 5.3 Android SDK 33, build tools, NDK r25b and platform tools.'
1..80 | ForEach-Object { 'y' } | & $manager "--sdk_root=$sdkRoot" --licenses *> (Join-Path $ToolchainRoot 'sdk-licenses.log')
if ($LASTEXITCODE -ne 0) { throw 'Android SDK license setup failed; see sdk-licenses.log.' }
1..80 | ForEach-Object { 'y' } | & $manager "--sdk_root=$sdkRoot" 'platform-tools' 'platforms;android-33' 'build-tools;33.0.1' 'ndk;25.1.8937393' 'cmake;3.10.2.4988404' *> (Join-Path $ToolchainRoot 'sdk-install.log')
if ($LASTEXITCODE -ne 0) { throw 'Android SDK package install failed; see sdk-install.log.' }
$manifest = [ordered]@{sdk=$sdkRoot;jdk=$javaInstall.FullName;ndk=$env:NDKROOT;commandline_tools='9.0';platform='android-33';build_tools='33.0.1';java_source=$javaPackage.link;java_sha256=$javaPackage.checksum;google_tools_sha1=[string]$archive.checksum}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $ToolchainRoot 'toolchain.json') -Encoding utf8
& (Join-Path $javaInstall.FullName 'bin\java.exe') -version
& (Join-Path $sdkRoot 'platform-tools\adb.exe') version
Write-Output "Android build dependencies installed in $ToolchainRoot. No persistent system environment was changed."
