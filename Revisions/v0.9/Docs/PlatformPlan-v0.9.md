# Platform and build prerequisite audit — v0.9

Initial audit: 2026-11-21. Readiness update: 2026-11-22. See the linked platform status records for artifact identities and latest verification. No physical device launch or clean-VM installer smoke has completed.

## Readiness

| Target | Local readiness | Actual blocker / scope |
|---|---|---|
| Windows x64 | **Current package regression passed 60 scene and 67 input checks; installer candidate compiled.** See [installer status](Installer-v0.9.md), [clean-install status](WindowsCleanInstall-v0.9.md), [artifact ledger](Evidence/v0.9/platform/death-cached-artifacts.json), [current Windows regression](Evidence/v0.9/rendering/death-cached-regression/), and [historical runtime/LAN evidence](RuntimeBuildReport.md). | The Windows guest has the OS installed but is paused at Administrator password setup before SetupComplete. No installer/game smoke or uninstall result. Existing LAN evidence is same-host loopback only and predates the current binary; physical-network behavior remains unverified. |
| Android | **APK rebuilt and payload verified.** Current archive metadata: min API 26, target API 33, arm64-v8a. See [Android status](Android-v0.9.md) and [artifact ledger](Evidence/v0.9/platform/death-cached-artifacts.json). | Emulators are installed but have never booted the game; no physical-device install/launch, rendering, touch, performance or mixed-platform LAN result exists. API 33 is the locally built artifact target; current Google Play compliance remains unverified. UE 5.3 toolchain guidance below is historical packaging context, not evidence of current store eligibility. |
| macOS / iOS | **No Apple artifact.** See [Apple platform status](Apple-v0.9.md). | No Mac/Xcode build or Apple device test is available; macOS/iOS packaging and signing remain outstanding. |

Platform references: [Epic's UE 5.3 Android setup](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-set-up-android-sdk-and-ndk-for-your-unreal-engine-development-environment?application_version=5.3), [Android requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/android-development-requirements-for-unreal-engine), [macOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine), [iOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/ios-ipados-and-tvos-development-requirements-for-unreal-engine), and [remote iOS build setup](https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-remote-builds-of-unreal-engine-projects-for-ios). UE 5.3's SDK/Xcode matrix describes that engine version's supported toolchain; it is historical context, not proof of current store compliance.

## Cross-platform LAN assumptions and code impact

The v0.9 project configures Unreal's IP net driver on UDP 7777 (`Game/Config/DefaultEngine.ini`). `RTSController.h` declares server RPCs for lobby/actions and client RPCs for filtered snapshots; the host-authoritative simulation and explicit host IPv4 join flow already give a reasonable LAN foundation across UE platforms. Windows automated LAN probes are reported as passing, but this does **not** prove Windows/macOS/Android/iPhone clients interoperate: physical multi-device and mixed-platform LAN remain unverified in `Docs/RuntimeBuildReport.md`.

Keep the same UE version, protocol/schema, game data IDs, port and compatible package across peers; validate serialization, packet sizing, IPv4/IPv6/network selection, Wi-Fi client isolation, and firewall behavior on real mixed-platform devices. Android source compilation and cooking are verified; Apple builds remain unverified. Touch selection and orders are implemented in `RTSTouchInput.cpp`, but their physical-device behavior, readability and performance still require testing. A successful compile alone does not establish mobile playability.

## Fresh-Windows installer boundary

The current Inno Setup candidate identity is recorded in [Installer-v0.9](Installer-v0.9.md) and the [final artifact ledger](Evidence/v0.9/platform/death-cached-artifacts.json). It packages the staged Windows game and Epic's `UEPrereqSetup_x64.exe`; its clean-VM install and game startup remain unverified. The Windows guest is paused before SetupComplete at Administrator password setup ([VM status](WindowsCleanInstall-v0.9.md)). Unreal Engine, VS, .NET SDK, Android SDK/JDK, and build tools are **developer build dependencies**, not player prerequisites. Package only runtime prerequisites permitted by Epic/Microsoft; Microsoft's VC++ redist redistribution is license-limited ([guidance](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files)). Signing and release certification remain outstanding.

## Recommended route

1. Resume the Windows guest through OOBE and run the clean offline installer/game smoke, then verify uninstall/save preservation ([VM status](WindowsCleanInstall-v0.9.md)).
2. Boot the game on physical Android hardware, validate renderer/touch/performance, and test mixed-platform LAN. Assess current store requirements separately from the existing API 33 Development artifact.
3. Secure access to a Mac with compatible Xcode; first validate native macOS build, then configure and prove remote iOS build/signing. Test LAN sessions on actual Mac, Android, and iPhone hardware and implement touch/mobile UI as a separate feature scope.
4. A browser/PWA multiplayer client could be a later secondary reach path, but it would be a separate client/product implementation; it cannot replace the requested native desktop and mobile builds.
