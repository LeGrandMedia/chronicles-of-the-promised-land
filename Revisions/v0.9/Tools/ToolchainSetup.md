# Local Unreal build toolchain

Unreal Build Tool can reuse a cached makefile environment from a previous process. In this workspace that cache has caused MSVC `D8050` failures across sandbox sessions. `Tools/Build.ps1` passes `-NoUBTMakefiles` so each build gets a fresh UBT makefile environment.

## .NET Framework SDK for UBT

Unreal Engine 5.3's `SwarmInterface` editor module requires a .NET Framework SDK. UBT's `MicrosoftPlatformSDK.TryGetNetFxSdkInstallDir` first checks its AutoSDK root for versions 4.6.2, 4.6.1, or 4.6, and looks for `Include/um/mscoree.h`.

This workspace uses the official [Microsoft .NET Framework 4.6.2 Developer Pack](https://dotnet.microsoft.com/en-us/download/dotnet-framework/net462), downloaded from Microsoft's `download.microsoft.com` host as `NDP462-DevPack-KB3151934-ENU.exe`. The package's Windows Authenticode signature was verified as `Valid`, publisher `Microsoft Corporation`. The SDK files were extracted into the workspace; the installer was not run and no registry or system setting was changed.

The SDK is in the ignored `Tools/Toolchain/AutoSDK` directory. Its key files are:

```text
Tools/Toolchain/AutoSDK/HostWin64/Win64/Windows Kits/NETFXSDK/4.6.2/Include/um/mscoree.h
Tools/Toolchain/AutoSDK/HostWin64/Win64/Windows Kits/NETFXSDK/4.6.2/Lib/um/x64/mscoree.lib
```

`Tools/Build.ps1` checks for the header and sets `UE_SDKS_ROOT` to the local AutoSDK directory only in its process and child build process. The setting does not persist outside that build. `Tools/Package.ps1` applies the same process-local root so UBT and Unreal Editor command-line cooking inherit it.

To reproduce the extraction from the official package without installing it:

1. Download the Developer Pack from Microsoft's page and verify it with `Get-AuthenticodeSignature` before use.
2. Extract the second embedded Microsoft Cabinet (`MSCF`) payload from the executable to `NetFx462DevPackPayload.cab`; use `expand -D` to inspect its members.
3. Extract the CAB with `expand -F:*` to a staging directory, then extract member `a7`, which contains the `SDK_netfx_headers_*_F` and `SDK_netfx_libs_amd64_*_F` files.
4. Copy each header after removing the `SDK_netfx_headers_` prefix and `_F` suffix into `Include/um`; copy each `SDK_netfx_libs_amd64_*_F` file after removing its prefix and suffix into `Lib/um/x64`.

The original installer, staging payloads, and AutoSDK files are excluded by `.gitignore`; they are local build dependencies and are not included in the packaged game.
