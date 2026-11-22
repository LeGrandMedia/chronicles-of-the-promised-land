; Chronicles of the Promised Land — Windows x64 installer
; Build with Tools/BuildInstaller.ps1 after Tools/Package.ps1 has produced v0.9.
#ifndef PackageDir
  #define PackageDir GetEnv("CHRONICLES_PACKAGE_DIR")
#endif
#if PackageDir == ""
  #define PackageDir AddBackslash(SourcePath) + "..\\Builds\\Windows"
#endif
#ifndef OutputDir
  #define OutputDir GetEnv("CHRONICLES_INSTALLER_OUTPUT")
#endif
#if OutputDir == ""
  #define OutputDir AddBackslash(SourcePath) + "..\\Builds\\Installer"
#endif
#ifndef AppVersion
  #define AppVersion GetEnv("CHRONICLES_APP_VERSION")
#endif
#if AppVersion == ""
  #define AppVersion "0.9.0"
#endif

#define AppName "Chronicles of the Promised Land"
#define AppExeName "Chronicles.exe"

[Setup]
AppId={{2D80B696-4E32-4980-A31E-190DAB51594A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Chronicles of the Promised Land
DefaultDirName={localappdata}\Programs\Chronicles
DefaultGroupName={#AppName}
UninstallDisplayName={#AppName} {#AppVersion}
UninstallDisplayIcon={app}\{#AppExeName}
Uninstallable=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=Chronicles-Setup-v{#AppVersion}-Windows-x64
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
DisableProgramGroupPage=yes
VersionInfoVersion=0.9.0.0
VersionInfoCompany=Chronicles of the Promised Land
VersionInfoDescription=Install {#AppName}
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: checkedonce

[Files]
; Embed the offline Epic prerequisite bootstrapper for PrepareToInstall, before app files are written.
Source: "{#PackageDir}\Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe"; Flags: dontcopy noencryption
; PackageDir is always the v0.9 staged Windows package; don't pull in source, caches, or test output.
Source: "{#PackageDir}\*"; DestDir: "{app}"; Excludes: "README.txt,*.pdb,*.log,*.tmp,*.bak,*.obj,*.ilk,*.exp,*.lib,*.pch,*.idb,*.ipch,*.sdf,*.VC.db,*.VC.VC.opendb,*.suo,*.user,Manifest_*.txt,Saved\*,Intermediate\*,DerivedDataCache\*,Source\*,Revisions\*,Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourcePath}\README.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{autoprograms}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  PrereqPath: String;
begin
  Result := '';
  NeedsRestart := False;
  try
    ExtractTemporaryFile('UEPrereqSetup_x64.exe');
  except
    Result := 'Could not extract the bundled offline Unreal runtime prerequisite installer. Check free disk space and retry setup.';
    exit;
  end;

  PrereqPath := ExpandConstant('{tmp}\UEPrereqSetup_x64.exe');
  if not Exec(PrereqPath, '/quiet /norestart', ExpandConstant('{tmp}'), SW_HIDE,
      ewWaitUntilTerminated, ResultCode) then begin
    Result := 'Could not start the bundled Unreal runtime prerequisite installer (' +
      SysErrorMessage(ResultCode) + '). Setup stopped before installing the game. Restart Windows if another setup is pending, then retry.';
    exit;
  end;

  if ResultCode = 0 then
    exit;

  { Windows Installer standard success-with-reboot-required / reboot-initiated codes. }
  if (ResultCode = 3010) or (ResultCode = 1641) then begin
    NeedsRestart := True;
    Result := 'Unreal runtime prerequisites installed, but Windows requires a restart. Restart the computer, then run this installer again. The game files were not installed yet.';
    exit;
  end;

  Result := 'The Unreal runtime prerequisite installer failed with exit code ' + IntToStr(ResultCode) +
    '. Setup stopped before installing the game. Close other installers, restart Windows if one is pending, and retry. If it fails again, contact support with this code.';
end;

