param([string]$OutputPath = "$PSScriptRoot/../Docs/Evidence/2026-10-20/capabilities.json")
$ErrorActionPreference = 'Continue'
$commands = foreach ($name in @('git','python','blender','UnrealEditor','UnrealBuildTool','cl','cmake','dotnet')) {
    $found = Get-Command $name -ErrorAction SilentlyContinue
    [ordered]@{name=$name; found=[bool]$found; path=if($found){$found.Source}else{$null}}
}
$enginePaths = @('C:/Program Files/Epic Games/UE5/UE_5.3','D:/Unreal Engine 5/fortnite/UE_5.1')
$engines = foreach($path in $enginePaths) {
    $versionPath = "$path/Engine/Build/Build.version"
    [ordered]@{path=$path; version=if(Test-Path -LiteralPath $versionPath){Get-Content -LiteralPath $versionPath -Raw | ConvertFrom-Json}else{$null}; editor_exists=(Test-Path -LiteralPath "$path/Engine/Binaries/Win64/UnrealEditor.exe"); build_script_exists=(Test-Path -LiteralPath "$path/Engine/Build/BatchFiles/Build.bat"); launched=$false}
}
$vswhere = 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = if(Test-Path -LiteralPath $vswhere){ & $vswhere -all -products '*' -format json | ConvertFrom-Json }else{@()}
$compilerPaths = @('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe','C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x64/cl.exe')
$compilers = foreach($path in $compilerPaths){[ordered]@{path=$path; exists=(Test-Path -LiteralPath $path); compiled=$false}}
$blenderPaths = @('C:/Program Files/Blender Foundation','D:/Program Files/Blender Foundation','D:/D program files/Blender Foundation')
$blender = foreach($path in $blenderPaths){[ordered]@{path=$path; exists=(Test-Path -LiteralPath $path); children=@(if(Test-Path -LiteralPath $path -PathType Container){Get-ChildItem -LiteralPath $path -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name})}}
$hardware = [ordered]@{processor_identifier=$env:PROCESSOR_IDENTIFIER; logical_processors=$env:NUMBER_OF_PROCESSORS; cim_status='Not available: all four initial Win32 hardware/OS CIM queries were denied'; physical_memory_bytes=$null}
try {$hardware.cpu_name=(Get-ItemPropertyValue -LiteralPath 'HKLM:/HARDWARE/DESCRIPTION/System/CentralProcessor/0' -Name ProcessorNameString -ErrorAction Stop).Trim()}catch{$hardware.cpu_error=$_.Exception.Message}
$hardware.gpu=@(Get-ItemProperty 'HKLM:/SYSTEM/CurrentControlSet/Control/Video/*/0000' -ErrorAction SilentlyContinue | Where-Object DriverDesc | ForEach-Object { [ordered]@{name=$_.DriverDesc; dedicated_memory_bytes=$_.'HardwareInformation.qwMemorySize'} })
$blenderRegistrations = @(Get-ItemProperty 'HKLM:/SOFTWARE/Microsoft/Windows/CurrentVersion/Uninstall/*','HKLM:/SOFTWARE/WOW6432Node/Microsoft/Windows/CurrentVersion/Uninstall/*','HKCU:/SOFTWARE/Microsoft/Windows/CurrentVersion/Uninstall/*' -ErrorAction SilentlyContinue | Where-Object {$_.DisplayName -like '*Blender*'} | Select-Object DisplayName,DisplayVersion,InstallLocation,DisplayIcon)
$blenderExecutables = @($blenderRegistrations | ForEach-Object {
    $candidate = Join-Path $_.InstallLocation 'blender.exe'
    if(Test-Path -LiteralPath $candidate){$item=Get-Item -LiteralPath $candidate; [ordered]@{path=$candidate; file_version=$item.VersionInfo.FileVersion; product_version=$item.VersionInfo.ProductVersion; launch_verified=$false}}
})
$result = [ordered]@{
    captured_utc=[DateTime]::UtcNow.ToString('o'); user_date='2026-10-20'; timezone='America/Phoenix'; os=[Environment]::OSVersion.VersionString
    commands=@($commands); engines=@($engines); visual_studio=@($visualStudio); compiler_candidates=@($compilers)
    sdk_include_versions=@(Get-ChildItem 'C:/Program Files (x86)/Windows Kits/10/Include' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
    blender_search=@($blender); blender_registrations=$blenderRegistrations; blender_executables=$blenderExecutables; hardware=$hardware
    drives=@(Get-PSDrive -PSProvider FileSystem | Select-Object Name,Used,Free)
    permissions='Workspace writes authorized; network restricted; no administrator/install/firewall privileges verified. No editor connector was exercised.'
    limitations='Discovery only: no compiler smoke test, Unreal launch, packaging, Blender launch, network test or performance benchmark performed. Explicit paths are discovery candidates, not a pinned build configuration.'
}
$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $parent -Force | Out-Null
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputPath -Encoding utf8
Write-Output "Capability discovery saved to $OutputPath"
