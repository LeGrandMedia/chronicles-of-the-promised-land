[CmdletBinding()]
param(
    [ValidateSet('Inspect', 'Prepare', 'StartOffline', 'Status', 'Screenshot', 'ReadResults')]
    [string]$Mode = 'Inspect',
    [string]$WindowsIso = 'D:\RTSTestVM\v0.9\ISO\WindowsServer2025_Eval_x64.iso',
    [string]$InstallerPath = (Join-Path $PSScriptRoot '..\Builds\Installer\GPU-20261122\Chronicles-Setup-v0.9.0-Windows-x64.exe'),
    [string]$VmRoot = 'D:\RTSTestVM\v0.9',
    [string]$Msys2Root = 'D:\RTSToolchains\MSYS2',
    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string]$InstallerSha256 = '33E6DC4A81BBD0A1089DE0910A7F29A48F5C67C20D1C0202B934BE66CE360D04',
    [ValidateRange(4, 32)][int]$MemoryGiB = 8,
    [ValidateRange(1, 4)][int]$Vcpu = 2,
    [ValidateRange(64, 256)][int]$DiskGiB = 100
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$vmRootPath = [IO.Path]::GetFullPath($VmRoot).TrimEnd('\')
if ($vmRootPath -notmatch '^[A-Za-z]:\\.+$') { throw 'Use a dedicated local directory for VmRoot.' }
$media = Join-Path $vmRootPath 'Media'
$results = Join-Path $vmRootPath 'Results'
$disk = Join-Path $vmRootPath 'Server2025-Eval.qcow2'
$testIso = Join-Path $vmRootPath 'RTS-Test-Media.iso'
$configPath = Join-Path $vmRootPath 'vm-config.json'
$runPath = Join-Path $results 'run.json'
$serialPath = Join-Path $results 'qemu-serial.log'
$installerName = 'Chronicles-Setup-v0.9.0-Windows-x64.exe'
$qemu = Join-Path $Msys2Root 'ucrt64\bin\qemu-system-x86_64.exe'
$qemuImg = Join-Path $Msys2Root 'ucrt64\bin\qemu-img.exe'
$pinnedIsoHash = '7B052573BA7894C9924E3E87BA732CCD354D18CB75A883EFA9B900EA125BFD51'

function Assert-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required file is missing: $Path" }
    $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($hash -ne $Expected) { throw "SHA-256 mismatch for $Path" }
}

function Get-EvaluationImage([string]$IsoPath) {
    $attached = Get-DiskImage -ImagePath $IsoPath
    $ownsMount = -not $attached.Attached
    if ($ownsMount) { $attached = Mount-DiskImage -ImagePath $IsoPath -Access ReadOnly -PassThru }
    try {
        $volume = $attached | Get-Volume
        if (-not $volume.DriveLetter) { throw 'ISO mount did not have a drive letter.' }
        $wim = '{0}:\sources\install.wim' -f $volume.DriveLetter
        $stream = [IO.File]::OpenRead($wim)
        try {
            $reader = [IO.BinaryReader]::new($stream)
            $header = $reader.ReadBytes(96)
            # The WIM header points to its UTF-16 XML image catalogue.
            if ($header.Length -ne 96 -or [Text.Encoding]::ASCII.GetString($header, 0, 5) -ne 'MSWIM') { throw 'Invalid install.wim header.' }
            $length = [BitConverter]::ToUInt64($header, 72) -band 0x00ffffffffffffff
            $offset = [BitConverter]::ToUInt64($header, 80)
            if ($length -eq 0 -or $length -gt 8MB -or $offset + $length -gt $stream.Length) { throw 'Invalid install.wim catalogue range.' }
            $stream.Position = $offset
            $xml = [xml]([Text.Encoding]::Unicode.GetString($reader.ReadBytes([int]$length)).Trim([char]0xFEFF, [char]0))
        } finally { $stream.Dispose() }
        $selected = @($xml.WIM.IMAGE | Where-Object {
            $_.WINDOWS.EDITIONID -eq 'ServerStandardEval' -and $_.WINDOWS.INSTALLATIONTYPE -eq 'Server' -and
                $_.WINDOWS.ARCH -eq '9' -and $_.WINDOWS.LANGUAGES.DEFAULT -eq 'en-US'
        })
        if ($selected.Count -ne 1) { throw 'Expected one x64 en-US Server Standard Evaluation Desktop Experience image.' }
        $image = $selected[0]
        [pscustomobject]@{
            Index = [int]$image.GetAttribute('INDEX')
            Name = $image.NAME
            Edition = $image.WINDOWS.EDITIONID
            InstallationType = $image.WINDOWS.INSTALLATIONTYPE
            Build = $image.WINDOWS.VERSION.BUILD
            Language = $image.WINDOWS.LANGUAGES.DEFAULT
        }
    } finally {
        if ($ownsMount) { Dismount-DiskImage -ImagePath $IsoPath | Out-Null }
    }
}

function Write-TestIso([string]$Source, [string]$Path) {
    if (-not ('RtsTestIsoWriter' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

public static class RtsTestIsoWriter {
    public static void Write(object source, string path) {
        var stream = (IStream)source;
        var buffer = new byte[1024 * 1024];
        var count = Marshal.AllocCoTaskMem(sizeof(int));
        try {
            using (var output = new FileStream(path, FileMode.CreateNew, FileAccess.Write)) {
                for (;;) {
                    stream.Read(buffer, buffer.Length, count);
                    int read = Marshal.ReadInt32(count);
                    if (read == 0) break;
                    output.Write(buffer, 0, read);
                }
            }
        } finally {
            Marshal.FreeCoTaskMem(count);
        }
    }
}
'@
    }
    $image = New-Object -ComObject IMAPI2FS.MsftFileSystemImage
    $image.VolumeName = 'RTSV09_MEDIA'
    $image.FileSystemsToCreate = 3 # ISO 9660 plus Joliet for long filenames.
    $image.FreeMediaBlocks = 2300000
    $image.Root.AddTree($Source, $false)
    $built = $image.CreateResultImage()
    [RtsTestIsoWriter]::Write($built.ImageStream, $Path)
}

function Write-TestMedia([int]$ImageIndex) {
    $hook = Join-Path $media 'VMSetup'
    New-Item -ItemType Directory -Path $hook | Out-Null
    $answer = @'
<?xml version="1.0" encoding="utf-8"?>
<unattend xmlns="urn:schemas-microsoft-com:unattend" xmlns:wcm="http://schemas.microsoft.com/WMIConfig/2002/State">
  <settings pass="windowsPE">
    <component name="Microsoft-Windows-International-Core-WinPE" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS">
      <SetupUILanguage><UILanguage>en-US</UILanguage></SetupUILanguage>
      <InputLocale>en-US</InputLocale><SystemLocale>en-US</SystemLocale>
      <UILanguage>en-US</UILanguage><UserLocale>en-US</UserLocale>
    </component>
    <component name="Microsoft-Windows-Setup" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS">
      <DiskConfiguration>
        <Disk wcm:action="add">
          <DiskID>0</DiskID><WillWipeDisk>true</WillWipeDisk>
          <CreatePartitions>
            <CreatePartition wcm:action="add"><Order>1</Order><Type>Primary</Type><Extend>true</Extend></CreatePartition>
          </CreatePartitions>
          <ModifyPartitions>
            <ModifyPartition wcm:action="add"><Order>1</Order><PartitionID>1</PartitionID><Active>true</Active><Format>NTFS</Format><Label>Windows</Label><Letter>C</Letter></ModifyPartition>
          </ModifyPartitions>
        </Disk>
        <WillShowUI>OnError</WillShowUI>
      </DiskConfiguration>
      <ImageInstall><OSImage>
        <InstallFrom><MetaData wcm:action="add"><Key>/IMAGE/INDEX</Key><Value>__IMAGE_INDEX__</Value></MetaData></InstallFrom>
        <InstallTo><DiskID>0</DiskID><PartitionID>1</PartitionID></InstallTo><WillShowUI>OnError</WillShowUI>
      </OSImage></ImageInstall>
      <UserData><AcceptEula>true</AcceptEula></UserData>
    </component>
  </settings>
  <settings pass="specialize">
    <component name="Microsoft-Windows-International-Core" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS">
      <InputLocale>en-US</InputLocale><SystemLocale>en-US</SystemLocale><UILanguage>en-US</UILanguage><UserLocale>en-US</UserLocale>
    </component>
    <component name="Microsoft-Windows-Deployment" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS">
      <RunSynchronous>
        <RunSynchronousCommand wcm:action="add">
          <Order>1</Order><Description>Stage the v0.9 LocalSystem test</Description>
          <Path>cmd.exe /d /c for %L in (D E F G H I J K L M N O P Q R S T U V W Y Z) do @if exist %L:\VMSetup\StageSmoke.cmd call %L:\VMSetup\StageSmoke.cmd</Path>
          <WillReboot>Never</WillReboot>
        </RunSynchronousCommand>
      </RunSynchronous>
    </component>
  </settings>
</unattend>
'@
    $answer = $answer.Replace('__IMAGE_INDEX__', [string]$ImageIndex)
    [void][xml]$answer
    Set-Content -LiteralPath (Join-Path $media 'Autounattend.xml') -Value $answer -Encoding utf8

    $stage = @'
@echo off
setlocal
if not exist "%WINDIR%\Setup\Scripts" mkdir "%WINDIR%\Setup\Scripts"
copy /y "%~dp0SetupComplete.cmd" "%WINDIR%\Setup\Scripts\SetupComplete.cmd" || exit /b 1
copy /y "%~dp0RunSmoke.ps1" "%WINDIR%\Setup\Scripts\RunSmoke.ps1" || exit /b 1
mode COM1: BAUD=115200 PARITY=N DATA=8 STOP=1 >nul 2>&1
echo RTSV09 SPECIALIZE_HOOK_STAGED>COM1
exit /b 0
'@
    Set-Content -LiteralPath (Join-Path $hook 'StageSmoke.cmd') -Value $stage -Encoding ascii

    $setupComplete = @'
@echo off
setlocal
set "LOGROOT=%SystemDrive%\RTSTest"
if not exist "%LOGROOT%" mkdir "%LOGROOT%"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -File "%~dp0RunSmoke.ps1" -LogRoot "%LOGROOT%" >"%LOGROOT%\RunSmoke-console.txt" 2>&1
set "SMOKE_EXIT=%ERRORLEVEL%"
echo RunSmoke exit code: %SMOKE_EXIT%>"%LOGROOT%\SetupComplete.txt"
if not "%SMOKE_EXIT%"=="0" echo RTSV09 HOOK_EXIT %SMOKE_EXIT%>COM1
exit /b 0
'@
    Set-Content -LiteralPath (Join-Path $hook 'SetupComplete.cmd') -Value $setupComplete -Encoding ascii

    $smoke = @'
param([Parameter(Mandatory)][string]$LogRoot)
$ErrorActionPreference = 'Stop'
$app = Join-Path $LogRoot 'Installed'
$installLog = Join-Path $LogRoot 'Installer-System.log'
$gameLog = Join-Path $LogRoot 'Chronicles-NullRHI.log'
$reportPath = Join-Path $LogRoot 'SystemBootstrapSmoke.json'
$vcKey = 'HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$report = [ordered]@{
    Test = 'Offline LocalSystem installer and NullRHI startup smoke'
    UserSid = $identity.User.Value
    StartedUtc = [datetime]::UtcNow.ToString('o')
    InstallerSha256 = $null
    InstallerExitCode = $null
    GameExitCode = $null
    EngineInitialized = $false
    EntryLoaded = $false
    Failure = $null
    Passed = $false
}

function Get-VcRuntime {
    if (Test-Path -LiteralPath $vcKey) {
        return Get-ItemProperty -LiteralPath $vcKey | Select-Object Installed, Version, Major, Minor, Bld, Rbld
    }
    [pscustomobject]@{ Installed = 0; Version = 'not registered' }
}

function Send-Evidence([string]$File) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { return }
    $bytes = [IO.File]::ReadAllBytes($File)
    if ($bytes.Length -gt 2MB) { throw "Serial evidence exceeds 2 MiB: $File" }
    $name = [IO.Path]::GetFileName($File)
    $hash = (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash
    $base64 = [Convert]::ToBase64String($bytes)
    $port.WriteLine("RTSV09 BEGIN $name $($bytes.Length) $hash")
    for ($i = 0; $i -lt $base64.Length; $i += 768) {
        $port.WriteLine('RTSV09 DATA ' + $base64.Substring($i, [Math]::Min(768, $base64.Length - $i)))
    }
    $port.WriteLine("RTSV09 END $name")
}

$port = [IO.Ports.SerialPort]::new('COM1', 115200, 'None', 8, 'One')
$port.WriteTimeout = 20000
try {
    $port.Open()
    $port.WriteLine('')
    $port.WriteLine('RTSV09 SETUPCOMPLETE')
    if ($identity.User.Value -ne 'S-1-5-18') { throw 'This smoke requires the Windows Setup LocalSystem context.' }
    $report.Windows = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' |
        Select-Object ProductName, EditionID, InstallationType, CurrentBuild
    if ($report.Windows.EditionID -ne 'ServerStandardEval' -or $report.Windows.InstallationType -ne 'Server') {
        throw 'The expected Windows Server Standard Evaluation Desktop Experience image did not boot.'
    }
    $report.VcX64Before = Get-VcRuntime
    $installer = $null
    foreach ($drive in [IO.DriveInfo]::GetDrives()) {
        if ($drive.DriveType -ne [IO.DriveType]::CDRom -or -not $drive.IsReady -or $drive.VolumeLabel -ne 'RTSV09_MEDIA') { continue }
        $candidate = Join-Path $drive.RootDirectory.FullName 'VMSetup\Chronicles-Setup-v0.9.0-Windows-x64.exe'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { $installer = $candidate; break }
    }
    if (-not $installer) { throw 'The read-only installer media was not found.' }
    $report.InstallerSha256 = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
    if ($report.InstallerSha256 -ne '__INSTALLER_HASH__') { throw 'The mounted installer hash is incorrect.' }
    $port.WriteLine('RTSV09 INSTALLER_BEGIN')
    $installArgs = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS', (('/DIR="{0}"' -f $app)), (('/LOG="{0}"' -f $installLog)))
    $process = Start-Process -FilePath $installer -ArgumentList $installArgs -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(3600000)) { throw 'Installer exceeded the 60-minute TCG budget. The guest process was left running for diagnosis.' }
    $report.InstallerExitCode = $process.ExitCode
    $report.VcX64After = Get-VcRuntime
    if ($process.ExitCode -ne 0) { throw "Installer exited $($process.ExitCode)." }

    $game = Join-Path $app 'Chronicles\Binaries\Win64\Chronicles.exe'
    if (-not (Test-Path -LiteralPath $game -PathType Leaf)) { throw "The installed game binary is missing: $game" }
    $logArg = '-abslog="{0}"' -f $gameLog
    $gameArgs = @('-NullRHI', '-Unattended', '-NoSound', '-NoSplash', '-ExecCmds=Quit', $logArg)
    $port.WriteLine('RTSV09 NULLRHI_BEGIN')
    $process = Start-Process -FilePath $game -ArgumentList $gameArgs -WorkingDirectory $app -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(600000)) {
        Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
        throw 'Installed game did not exit within the ten-minute NullRHI budget.'
    }
    $report.GameExitCode = $process.ExitCode
    if (Test-Path -LiteralPath $gameLog) {
        $trace = Get-Content -LiteralPath $gameLog -Raw
        $report.EngineInitialized = $trace -match 'Engine is initialized'
        $report.EntryLoaded = $trace -match 'LoadMap\(/Engine/Maps/Entry\)'
    }
    if ($process.ExitCode -ne 0 -or -not $report.EngineInitialized -or -not $report.EntryLoaded) {
        throw 'The installed game did not complete the expected NullRHI launch and map load.'
    }
    $report.Passed = $true
} catch {
    $report.Failure = $_.Exception.Message
} finally {
    $report.FinishedUtc = [datetime]::UtcNow.ToString('o')
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding utf8
    if ($port.IsOpen) {
        try {
            Send-Evidence $reportPath
            Send-Evidence $installLog
            Send-Evidence $gameLog
            $setup = Join-Path $env:WINDIR 'Panther\setupact.log'
            if (Test-Path -LiteralPath $setup) {
                $tail = Join-Path $LogRoot 'Setupact-tail.log'
                Get-Content -LiteralPath $setup -Tail 250 | Set-Content -LiteralPath $tail -Encoding utf8
                Send-Evidence $tail
            }
            $port.WriteLine('RTSV09 FINISHED ' + $report.Passed.ToString().ToLowerInvariant())
        } finally { $port.Dispose() }
    }
}
if (-not $report.Passed) { exit 1 }
'@
    Set-Content -LiteralPath (Join-Path $hook 'RunSmoke.ps1') -Value ($smoke.Replace('__INSTALLER_HASH__', $InstallerSha256)) -Encoding utf8
}

function Invoke-Qmp([int]$Port, [string]$Command, [hashtable]$Parameters = @{}) {
    if ($Port -lt 1024 -or $Port -gt 65535) { throw 'Unexpected local QMP port.' }
    $client = [Net.Sockets.TcpClient]::new()
    try {
        $connect = $client.ConnectAsync([Net.IPAddress]::Loopback, $Port)
        if (-not $connect.Wait(3000)) { throw 'QMP did not accept a local connection.' }
        $stream = $client.GetStream()
        $reader = [IO.StreamReader]::new($stream)
        $writer = [IO.StreamWriter]::new($stream, [Text.UTF8Encoding]::new($false))
        $writer.AutoFlush = $true
        function Receive-Qmp {
            $read = $reader.ReadLineAsync()
            if (-not $read.Wait(5000) -or -not $read.Result) { throw 'QMP did not return a line.' }
            $read.Result | ConvertFrom-Json
        }
        $greeting = Receive-Qmp
        if (-not $greeting.PSObject.Properties['QMP']) { throw 'The local endpoint is not a QMP server.' }
        $writer.WriteLine('{"execute":"qmp_capabilities"}')
        $capabilities = Receive-Qmp
        if ($capabilities.PSObject.Properties['error']) { throw ($capabilities.error | ConvertTo-Json -Compress) }
        $writer.WriteLine((@{ execute = $Command; arguments = $Parameters; id = 'RTS' } | ConvertTo-Json -Compress))
        do { $reply = Receive-Qmp } until ($reply.PSObject.Properties['id'] -and $reply.id -eq 'RTS')
        if ($reply.PSObject.Properties['error']) { throw ($reply.error | ConvertTo-Json -Compress) }
        return $reply.'return'
    } finally { $client.Dispose() }
}

if ($Mode -eq 'Inspect') {
    $iso = [IO.Path]::GetFullPath($WindowsIso)
    Assert-Hash $iso $pinnedIsoHash
    Get-EvaluationImage $iso
    return
}

if ($Mode -eq 'Prepare') {
    if (Test-Path -LiteralPath $disk) { throw "Refusing to overwrite $disk. Select an unused VM directory." }
    foreach ($path in @($media, $testIso, $configPath, $runPath)) {
        if (Test-Path -LiteralPath $path) { throw "Refusing to replace existing VM inputs at $path" }
    }
    if (-not (Test-Path -LiteralPath $qemu -PathType Leaf) -or -not (Test-Path -LiteralPath $qemuImg -PathType Leaf)) {
        throw 'Install and verify the scoped QEMU dependencies with SetupQemu-v09.ps1.'
    }
    $iso = [IO.Path]::GetFullPath($WindowsIso)
    $installer = [IO.Path]::GetFullPath($InstallerPath)
    Assert-Hash $iso $pinnedIsoHash
    Assert-Hash $installer $InstallerSha256
    $image = Get-EvaluationImage $iso
    New-Item -ItemType Directory -Path $media, $results -Force | Out-Null
    Write-TestMedia $image.Index
    Copy-Item -LiteralPath $installer -Destination (Join-Path $media "VMSetup\$installerName")
    Write-TestIso $media $testIso
    & $qemuImg create -f qcow2 $disk ("{0}G" -f $DiskGiB)
    if ($LASTEXITCODE -ne 0) { throw "Failed to create the new QCOW2 disk ($LASTEXITCODE)." }
    [pscustomobject]@{
        Image = $image
        WindowsIso = $iso
        WindowsIsoSha256 = $pinnedIsoHash
        Installer = $installer
        InstallerSha256 = $InstallerSha256
        TestMediaSha256 = (Get-FileHash -LiteralPath $testIso -Algorithm SHA256).Hash
        Disk = $disk
        DiskSizeGiB = $DiskGiB
        MemoryGiB = $MemoryGiB
        Vcpu = $Vcpu
        PreparedUtc = [datetime]::UtcNow.ToString('o')
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $configPath -Encoding utf8
    Write-Host "Prepared an unused QCOW2 disk and read-only optical test media at $vmRootPath"
    return
}

if ($Mode -eq 'ReadResults') {
    if (-not (Test-Path -LiteralPath $serialPath -PathType Leaf)) { throw 'No serial log is available.' }
    $log = [string](Get-Content -LiteralPath $serialPath -Raw)
    $allowed = 'SystemBootstrapSmoke.json', 'Installer-System.log', 'Chronicles-NullRHI.log', 'Setupact-tail.log'
    $pattern = '(?m)^RTSV09 BEGIN (?<name>[A-Za-z0-9.-]+) (?<size>[0-9]+) (?<hash>[A-F0-9]{64})\r?\n(?<data>(?:RTSV09 DATA [A-Za-z0-9+/=]+\r?\n)*)RTSV09 END \k<name>\r?$'
    foreach ($entry in [regex]::Matches($log, $pattern)) {
        $name = $entry.Groups['name'].Value
        if ($name -notin $allowed -or [long]$entry.Groups['size'].Value -gt 2MB) { continue }
        $encoded = $entry.Groups['data'].Value -replace '(?m)^RTSV09 DATA ', ''
        $bytes = [Convert]::FromBase64String($encoded)
        $hasher = [Security.Cryptography.SHA256]::Create()
        try { $hash = [BitConverter]::ToString($hasher.ComputeHash($bytes)).Replace('-', '') } finally { $hasher.Dispose() }
        if ($bytes.Length -ne [int]$entry.Groups['size'].Value -or $hash -ne $entry.Groups['hash'].Value) {
            throw "Serial evidence integrity check failed for $name"
        }
        [IO.File]::WriteAllBytes((Join-Path $results $name), $bytes)
    }
    $report = Join-Path $results 'SystemBootstrapSmoke.json'
    if ($log -notmatch '(?m)^RTSV09 FINISHED (true|false)\r?$') { Write-Host 'Guest smoke is pending or did not reach its completion marker.' }
    if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw }
    return
}

if ($Mode -in 'Status', 'Screenshot') {
    $run = Get-Content -LiteralPath $runPath -Raw | ConvertFrom-Json
    if ($Mode -eq 'Status') { Invoke-Qmp $run.QmpPort 'query-status'; return }
    $screen = Join-Path $results ('guest-' + [datetime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '.png')
    Invoke-Qmp $run.QmpPort 'screendump' @{ filename = $screen; format = 'png' } | Out-Null
    Get-Item -LiteralPath $screen
    return
}

if (Test-Path -LiteralPath $runPath) { throw 'This one-shot installation has already started. Its disk and evidence are retained.' }
$config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
if ($config.Disk -ne $disk -or -not (Test-Path -LiteralPath $disk -PathType Leaf)) { throw 'The expected dedicated VM disk is missing.' }
Assert-Hash $config.WindowsIso $pinnedIsoHash
Assert-Hash $testIso $config.TestMediaSha256
$diskInfo = & $qemuImg info --output=json $disk
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the QCOW2 disk.' }
$diskInfo = $diskInfo | ConvertFrom-Json
if ($diskInfo.format -ne 'qcow2' -or $diskInfo.PSObject.Properties['backing-filename'] -or
    $diskInfo.'format-specific'.data.PSObject.Properties['data-file']) { throw 'The VM must use a stand-alone QCOW2 disk.' }

# QEMU's Windows named-pipe backend blocks startup until a client connects.
$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
try {
    $listener.Start()
    $qmpPort = $listener.LocalEndpoint.Port
} finally { $listener.Stop() }
$arguments = @(
    '-name', 'Chronicles-v0.9-clean-install',
    '-machine', 'pc', '-accel', 'tcg,thread=multi', '-cpu', 'max',
    '-smp', [string]$config.Vcpu, '-m', ("{0}G" -f $config.MemoryGiB),
    '-drive', ("file={0},if=ide,index=0,format=qcow2" -f $disk.Replace(',', ',,')),
    '-drive', ("file={0},if=ide,index=2,media=cdrom,readonly=on,format=raw" -f $config.WindowsIso.Replace(',', ',,')),
    '-drive', ("file={0},if=ide,index=3,media=cdrom,readonly=on,format=raw" -f $testIso.Replace(',', ',,')),
    '-boot', 'order=c,once=d,menu=off', '-nic', 'none', '-display', 'none', '-vga', 'std', '-monitor', 'none',
    '-qmp', ("tcp:127.0.0.1:{0},server=on,wait=off" -f $qmpPort), '-serial', ("file:{0}" -f $serialPath)
)
$quoted = @($arguments | ForEach-Object { '"{0}"' -f $_ })
$process = Start-Process -FilePath $qemu -ArgumentList $quoted -WorkingDirectory $vmRootPath -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $results 'qemu.stdout.log') -RedirectStandardError (Join-Path $results 'qemu.stderr.log')
if ($process.WaitForExit(2000)) { throw "QEMU exited $($process.ExitCode) at startup. See Results\qemu.stderr.log." }
[pscustomobject]@{
    Pid = $process.Id
    QmpPort = $qmpPort
    SerialLog = $serialPath
    StartedUtc = [datetime]::UtcNow.ToString('o')
} | ConvertTo-Json | Set-Content -LiteralPath $runPath -Encoding utf8
Get-Content -LiteralPath $runPath -Raw
