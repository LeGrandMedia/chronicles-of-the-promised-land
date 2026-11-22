# Windows installer — v0.9

The installer source is `Installer/Chronicles.iss`; the build wrapper is `Tools/BuildInstaller.ps1`. The wrapper only compiles the staged v0.9 Windows package. It never invokes Unreal Build Tool, cooks content, runs the installer, or launches the game.

## Build

First run `Tools/Package.ps1` to produce `Builds/Windows`. The staged package must contain `Chronicles.exe` and `Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe`. Then install Inno Setup 7 from the [official download page](https://jrsoftware.org/inno-setup/downloads) and run:

```powershell
cd "D:\book of mormon RTS\Revisions\v0.9"
.\Tools\BuildInstaller.ps1
```

The output is `Builds/Installer/Chronicles-Setup-v0.9.0-Windows-x64.exe`. To use a nonstandard compiler location, pass `-CompilerPath 'C:\path\to\ISCC.exe'`. `ISCC.exe` may also be placed at `Tools/Toolchain/InnoSetup7/ISCC.exe`; that local dependency directory is ignored by git. The downloaded Inno Setup 7.1.1 installer used for this authoring task was signature-verified and installed in that ignored workspace-local directory; no system-wide tool installation was needed. Check Inno Setup's current license for the intended distribution/commercial use before release.

## Installer behavior

- Installs the already staged Windows x64 game to the current user's Local AppData by default, so the game's `Chronicles/Saved` settings directory remains writable without granting broad permissions to shared program folders.
- Embeds Epic's bundled `UEPrereqSetup_x64.exe` in the offline installer, extracts it to Inno's temporary directory, and runs it in `PrepareToInstall` before copying game files. The bootstrapper runs with `/quiet /norestart`; the bootstrapper may request UAC if a machine-wide runtime is missing. No Unreal Editor, compiler, Android SDK/JDK, or developer tools are installed for players.
- Checks the prerequisite process start result and exit code. Exit `0` continues. Windows Installer's documented success-with-reboot codes `3010` (restart required) and `1641` (restart initiated) stop setup with an explicit restart-and-rerun message before game files are installed. Any start failure or other exit code stops setup with a retryable error containing the code; there is no shipping bypass. Epic documents when to run this prerequisite installer but does not publish a UEPrereqSetup-specific exit-code table, so only standard Windows Installer success/reboot codes are accepted. If Epic's bootstrapper uses a different documented success code in a future package, update this allowlist based on that documentation before shipping.
- Adds a Start Menu shortcut and offers a checked-by-default Desktop shortcut. Windows Add/Remove Programs and an uninstall shortcut are provided.
- Does not add firewall rules. Windows Firewall may prompt when a player hosts; the player should allow the game on a Private network if appropriate.
- The source filter omits PDBs, intermediate/build caches, test `Saved` content, manifests, and prior revision directories. BuildInstaller refuses staged debug symbols; generated Unreal manifests are filtered from installer contents.
- Runtime-created files under `Chronicles/Saved` are not part of the installer file list and are left in place on uninstall. This preserves local settings; a leftover Saved directory may remain and can be deleted manually. `Saved` is excluded on upgrades so existing settings are not replaced by package content.

## Validation status and limitations

The final handoff installer is `Builds/Installer/Final-Handoff-20261122/Chronicles-Setup-v0.9.0-Windows-x64.exe` (524,314,833 bytes; SHA-256 `7CA6D6B7A4C2311D864F69C3A74BB6ADFDC585B07AAEA7317F0A9091B813F67D`). Its Windows executable is `7F30C0F4543A16277C60D389162F7A609B0E79A5AB3494C7BB77F5E8F4BAB9D4`. The final Windows executable is `7F30C0F4543A16277C60D389162F7A609B0E79A5AB3494C7BB77F5E8F4BAB9D4`. It passed 4,234 map checks, all 19 native reports, 46 gate-navigation cases, 57 bot assertions and 17 entity-visibility checks. Scene tests passed 86 cases and input tests passed 67 cases at each of 1600x900 and 888x500. Windows touch preview passed 26 cases. The mixed host/client/bot Narrow Neck LAN run passed on the same computer. Android verified all 754 payload hashes, 259 model mappings and 147 mesh/skeleton pairs. These results do not establish physical-network, mobile-device or clean-install behavior. The installer itself compiled successfully; no successful clean-install test is claimed. [Final ledger](Evidence/v0.9/platform/final-handoff-artifacts.json) · [Final regression](Evidence/v0.9/maps/final-handoff/) · [Final LAN](Evidence/v0.9/network/final-handoff/run.json).

The earlier host smoke was intentionally cancelled by stopping its task-owned Epic prerequisite bootstrapper while permission was pending. The prerequisite returned -1, setup exited 7, and no game executable was copied. This verifies the prerequisite failure guard, not installation success; no permission prompt was approved. See [host cancellation evidence](Evidence/v0.9/platform/host-prerequisite-cancelled/result.json).

The offline Windows Server evaluation VM installed Windows and staged the smoke hook. It reached required Administrator-password setup before SetupComplete, so the game installer did not run. The paused VM was closed during final handoff cleanup, preserving its disk and evidence. Its pinned candidate is the earlier `Builds/Installer/GPU-20261122/Chronicles-Setup-v0.9.0-Windows-x64.exe` (524,097,278 bytes; SHA-256 `33E6DC4A81BBD0A1089DE0910A7F29A48F5C67C20D1C0202B934BE66CE360D04`). That candidate predates the current maps, balance and raid/opening package and must not be described as testing the current installer. See [Windows clean-install status](WindowsCleanInstall-v0.9.md) for the paused VM and evidence. This Server/SYSTEM test cannot establish normal-user Windows 11 elevation or interactive rendering behavior.

No successful clean-machine install, game launch, upgrade, uninstall, or save-preservation test has been recorded. Before distribution, reach the VM smoke hook on a clean Windows x64 guest with no Unreal/VS toolchain; verify offline setup with runtimes present and missing, prerequisite success/reboot/failure handling, launch, host/join, shortcut creation, upgrade while preserving settings, and uninstall while retaining user data. An interactive Windows client run is still needed to validate Start Menu/Desktop shortcuts, player-visible elevation and save locations. The executable is unsigned; Windows security prompts may appear.

Windows 10 build 17763 or later, 64-bit, is required. macOS, Android, and iPhone packages are not included; see [PlatformPlan-v0.9](PlatformPlan-v0.9.md).

## Authoritative tool references

- [Inno Setup downloads](https://jrsoftware.org/inno-setup/downloads) and [Inno Setup `[Files]` documentation](https://jrsoftware.org/inno-setup/documentation/topic_filessection.htm).
- Inno Setup [`ExtractTemporaryFile`](https://jrsoftware.org/inno-setup/documentation/topic_isxfunc_extracttemporaryfile.htm), [`Exec`](https://jrsoftware.org/inno-setup/documentation/topic_isxfunc_exec.htm), [`PrepareToInstall`](https://jrsoftware.org/inno-setup/documentation/topic_scriptevents.htm), and [Setup exit codes](https://jrsoftware.org/inno-setup/documentation/topic_setupexitcodes.htm).
- Epic's [UEPrereq troubleshooting guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-game-sync-troubleshooting) identifies `UEPrereqSetup_x64.exe` as the Windows runtime prerequisite installer.
- Microsoft's [Windows Installer error codes](https://learn.microsoft.com/en-us/windows/win32/msi/error-codes) defines exit codes 0, 1641 and 3010 as successful outcomes, with 1641/3010 indicating restart behavior.
- Microsoft's [Visual C++ redistribution guidance](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files) describes redistribution limits; the project includes Epic's signed prerequisite package instead of assembling Microsoft runtime binaries manually.
