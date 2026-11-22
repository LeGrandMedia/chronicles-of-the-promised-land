[CmdletBinding()]
param(
    [string]$Msys2Root = 'D:\RTSToolchains\MSYS2',
    [string]$DownloadRoot = 'D:\RTSToolchains\Downloads\MSYS2-20261120',
    [string]$Gpg = 'C:\Program Files\Git\usr\bin\gpg.exe'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = [IO.Path]::GetFullPath($Msys2Root)
$downloads = [IO.Path]::GetFullPath($DownloadRoot)
$archiveName = 'msys2-base-x86_64-20261120.tar.zst'
$archiveUrl = "https://repo.msys2.org/distrib/x86_64/$archiveName"
$checksumUrl = "https://github.com/msys2/msys2-installer/releases/download/2026-11-20/$archiveName.sha256"
$fingerprint = '0EBF782C5D53F7E5FB02A66746BD761F7A49B0EC'
$keyUrl = "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x$fingerprint"
$archive = Join-Path $downloads $archiveName
$keyFile = Join-Path $downloads 'msys2-release-key.asc'
$keyring = Join-Path $downloads 'msys2-release-key.gpg'
$gpgv = Join-Path (Split-Path -Parent $Gpg) 'gpgv.exe'
$packageName = 'mingw-w64-ucrt-x86_64-qemu'
$qemu = Join-Path $root 'ucrt64\bin\qemu-system-x86_64.exe'
$qemuImg = Join-Path $root 'ucrt64\bin\qemu-img.exe'
$baseReceipt = Join-Path $root 'chronicles-msys2-base.sha256'
$installedEvidence = Join-Path $root 'chronicles-qemu-install.json'

function Save-Download([string]$Url, [string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) { return }
    & curl.exe --fail --location --silent --show-error --retry 3 --output "$Path.part" $Url
    if ($LASTEXITCODE -ne 0) { throw "Download failed for $Url" }
    Move-Item -LiteralPath "$Path.part" -Destination $Path
}

function Invoke-Msys2([string]$Command, [string]$LogName) {
    $bash = Join-Path $root 'usr\bin\bash.exe'
    $stdout = Join-Path $downloads "$LogName.log"
    $stderr = Join-Path $downloads "$LogName.stderr.log"
    $start = @{
        FilePath = $bash
        ArgumentList = @('--login', '-c', ('"{0}"' -f $Command))
        WindowStyle = 'Hidden'
        WorkingDirectory = $root
        RedirectStandardOutput = $stdout
        RedirectStandardError = $stderr
        PassThru = $true
    }
    $process = Start-Process @start
    # Bash can leave the scoped GnuPG agent running. Wait for bash, not the whole process tree.
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "MSYS2 exited $($process.ExitCode). See $stdout and $stderr" }
    Get-Content -LiteralPath $stdout
}

if (Test-Path -LiteralPath $installedEvidence -PathType Leaf) {
    if ((Test-Path -LiteralPath $qemu -PathType Leaf) -and (Test-Path -LiteralPath $qemuImg -PathType Leaf)) {
        Get-Content -LiteralPath $installedEvidence
        return
    }
    throw "The QEMU install receipt exists but a required binary is missing under $root. Select a fresh tooling path."
}
if ($root -notmatch '^[A-Za-z]:\\[A-Za-z0-9_.\\-]+$') { throw 'Use a short, local MSYS2 installation path without spaces.' }
if ((Test-Path -LiteralPath $root) -and -not (Test-Path -LiteralPath $baseReceipt -PathType Leaf)) {
    throw "Choose an unused MSYS2 path. Refusing to modify the existing directory $root"
}
if (-not (Test-Path -LiteralPath $Gpg -PathType Leaf) -or -not (Test-Path -LiteralPath $gpgv -PathType Leaf)) { throw "GnuPG verifier is missing alongside $Gpg" }
New-Item -ItemType Directory -Path $downloads -Force | Out-Null
Save-Download $archiveUrl $archive
Save-Download $checksumUrl "$archive.sha256"
Save-Download "$archiveUrl.sig" "$archive.sig"
Save-Download $keyUrl $keyFile

$checksum = (Get-Content -LiteralPath "$archive.sha256" -Raw).Trim()
if ($checksum -notmatch '^([A-Fa-f0-9]{64})(\s|$)') { throw 'The official archive checksum file has an unexpected format.' }
$actualHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
if ($actualHash -ne $Matches[1]) { throw 'MSYS2 archive SHA-256 does not match the official checksum.' }
& $Gpg --no-options --batch --yes --dearmor --output $keyring $keyFile
if ($LASTEXITCODE -ne 0) { throw 'Cannot decode the MSYS2 release key.' }
$gitCygpath = Join-Path (Split-Path -Parent $Gpg) 'cygpath.exe'
$verifierKeyring = $keyring
if (Test-Path -LiteralPath $gitCygpath -PathType Leaf) {
    $verifierKeyring = & $gitCygpath -u $keyring
    if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve the Git for Windows keyring path.' }
}
$signature = & $gpgv --keyring $verifierKeyring --status-fd 1 "$archive.sig" $archive
$signatureExit = $LASTEXITCODE
$validSignature = @($signature | Where-Object { $_ -match ('^\[GNUPG:\] VALIDSIG .+ ' + $fingerprint + '$') })
if ($signatureExit -ne 0 -or $validSignature.Count -ne 1) { throw 'MSYS2 archive did not verify against the pinned primary release key.' }
$signature | Set-Content -LiteralPath (Join-Path $downloads 'archive-signature.log') -Encoding ascii

if (Test-Path -LiteralPath $baseReceipt -PathType Leaf) {
    if ((Get-Content -LiteralPath $baseReceipt -Raw).Trim() -ne $actualHash) { throw 'The scoped base receipt belongs to another archive.' }
} else {
    New-Item -ItemType Directory -Path $root | Out-Null
    & tar.exe -xf $archive -C $root --strip-components 1
    if ($LASTEXITCODE -ne 0) { throw "Could not extract the signed base archive into $root" }
    Set-Content -LiteralPath $baseReceipt -Value $actualHash -Encoding ascii
}

$env:MSYSTEM = 'UCRT64'
$env:MSYS2_PATH_TYPE = 'strict'
$env:CHERE_INVOKING = '1'
Invoke-Msys2 'pacman-key --init && pacman-key --populate msys2' 'pacman-keyring' | Select-Object -Last 12
Invoke-Msys2 'pacman -Syu --noconfirm' 'pacman-core-update' | Select-Object -Last 12
Invoke-Msys2 "pacman -Syu --needed --noconfirm $packageName" 'pacman-qemu-install' | Select-Object -Last 18
$packages = @(Invoke-Msys2 "pacman -Q $packageName msys2-keyring" 'pacman-package-versions')
if (-not (Test-Path -LiteralPath $qemu -PathType Leaf) -or -not (Test-Path -LiteralPath $qemuImg -PathType Leaf)) { throw 'Signed package installation did not provide the expected QEMU binaries.' }
$version = & $qemu --version
if ($LASTEXITCODE -ne 0) { throw 'QEMU version check failed.' }
$diskVersion = & $qemuImg --version
if ($LASTEXITCODE -ne 0) { throw 'qemu-img version check failed.' }
$record = [pscustomobject]@{
    InstalledUtc = [datetime]::UtcNow.ToString('o')
    Root = $root
    ArchiveUrl = $archiveUrl
    ArchiveSha256 = $actualHash
    ChecksumUrl = $checksumUrl
    ReleaseSigningKey = $fingerprint
    Packages = $packages
    QemuVersion = $version[0]
    QemuImgVersion = $diskVersion[0]
    InstallLog = (Join-Path $downloads 'pacman-qemu-install.log')
}
$record | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $installedEvidence -Encoding utf8
Get-Content -LiteralPath $installedEvidence
