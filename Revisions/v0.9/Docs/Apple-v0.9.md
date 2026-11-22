# Apple native build handoff — v0.9

## Current status

No Mac, Xcode, Apple signing identity, provisioning profile, or iPhone is available in this build environment. **No macOS app or iOS IPA has been built.** The scripts below define a guarded local-Mac route and check their platform/signing inputs before invoking Unreal Automation Tool. Static inspection of scripts/configuration is not evidence of an Apple build or runtime test.

UE 5.3's published matrix for the 5.2–5.3 line lists macOS 12.5–13 and Xcode 14.1 / the Xcode 14 line; UE 5.3 supports iOS 15+ devices. The scripts deliberately pin Xcode 14.x for a repeatable UE 5.3 attempt and refuse other major versions. [Epic UE 5.3 iOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/ios-ipados-and-tvos-development-requirements-for-unreal-engine?application_version=5.3) · [Epic UE 5.3 macOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine?application_version=5.3) · [UE 5.3 release notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5.3-release-notes?application_version=5.3).

That legacy toolchain does not meet current iOS App Store upload requirements: as of April 28, 2026 Apple requires Xcode 26 or newer and iOS 26 SDK for App Store Connect uploads. Treat the iOS artifact from this route as a development sideload package only. A newer Unreal version/toolchain and separate Apple distribution/signing work are needed for store release. [Apple SDK minimum requirements](https://developer.apple.com/news/upcoming-requirements/?id=04282026a).

## Package on a Mac

Prepare the source with PowerShell from this revision root:

```powershell
.\Tools\Prepare-AppleHandoff.ps1
```

That command checks the allowlisted project/source/content/config inputs without creating an archive. To produce a handoff ZIP and a SHA-256 source manifest, run:

```powershell
.\Tools\Prepare-AppleHandoff.ps1 -CreateArchive
```

The ZIP includes the UE project file, all `Game/Source` and `Game/Content` (including `Content/Data`, model assets and 2D UI icons), default project settings, Apple configs, the two package scripts and this note. It excludes `Builds`, UE binaries, toolchains, caches, `Saved`, `Intermediate`, and signing credentials/profiles. The Mac must separately have an authorized UE 5.3 installation; Epic Engine binaries are not redistributed in this source archive.

On the Mac, extract the source handoff, install/authorize UE 5.3 from Epic, install/select Xcode 14.x, accept Xcode's license, and run:

```bash
bash Tools/Package-Mac.sh --engine-root "/path/to/UE_5.3"
```

The script invokes `BuildCookRun` for Mac Development, compiles the game, cooks project content, stages the app and archives it under `Builds/Apple/Mac/v0.9/Archive`. It validates the UE association, `RunUAT.sh`, Mac editor commandlet, Xcode and macOS version, and confirms one `.app` was produced. It does not sign for external distribution or notarize. To distribute outside the Mac App Store, add a Developer ID Application signing workflow and notarize/staple the app. Apple identifies Developer ID signing and notarization as the Gatekeeper path for externally distributed Mac apps. [Apple Developer ID](https://developer.apple.com/developer-id/) · [Notarizing macOS software](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).

## Package iOS for sideload testing

First register the configured bundle identifier with the Apple Developer team and create an iOS Development profile containing a registered test device and the matching Apple Development certificate. Install the certificate in the Mac keychain and the profile in Xcode/`~/Library/MobileDevice/Provisioning Profiles`. Apple documents those as prerequisites for a manual development profile. [Apple development provisioning profiles](https://developer.apple.com/help/account/provisioning-profiles/create-a-development-provisioning-profile).

Set the required variables in the Mac shell without writing them into the project or source handoff:

```bash
export APPLE_SIGNING_IDENTITY='Apple Development: …'
export APPLE_PROVISIONING_PROFILE='profile-name.mobileprovision'
export APPLE_TEAM_ID='ABCDEFGHIJ'
bash Tools/Package-iOS.sh --engine-root "/path/to/UE_5.3"
```

The script checks Xcode 14.x and the iPhoneOS SDK; confirms the exact certificate is in the keychain; decodes the selected installed profile in a temporary directory; and checks the team ID, exact bundle ID or a valid matching wildcard, registered-device presence, development `get-task-allow` entitlement, and future expiration date without printing profile contents. It passes the certificate/profile/team to UAT and verifies the resulting IPA's signature and bundle ID. It packages a Development IPA only, never installs or launches it. Development-profile creation requires an Apple App ID, certificate and registered device. [Apple profile prerequisites](https://developer.apple.com/help/account/provisioning-profiles/create-a-development-provisioning-profile).

The default bundle IDs in `Game/Config/IOS/IOSEngine.ini` and `Game/Config/Mac/MacEngine.ini` are project values, not proof of ownership or registration; change them to IDs owned by the chosen Apple team before signing if needed. Never place `.p12`, `.mobileprovision`, private keys, passwords, Apple account values or team credentials in the handoff ZIP or source tree.

## Platform consistency and unverified work

The iOS config targets iOS 15+, landscape, and the same `Chronicles` v0.9 package version. Both platform builds read the same authoritative server/client simulation and `DefaultEngine.ini` LAN net driver (UDP 7777). `DefaultGame.ini` stages the full `Data` directory loose; the handoff check enforces this and includes all project content so JSON catalogs, visual/model data and 2D icons travel with the game. No platform-specific network protocol or data IDs are introduced.

No macOS package, iOS IPA, signature, launch, performance, touch interaction, or mixed-platform LAN test is currently evidenced. After a Mac and iPhone are available, record tool versions and hashes, install/launch each build, verify all assets and touch interactions, then test host/client LAN sessions with Windows and Android using UDP 7777. Do not describe a successful script preflight or package configuration as a shipped native build.
