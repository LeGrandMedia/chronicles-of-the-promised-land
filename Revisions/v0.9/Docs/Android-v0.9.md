# Android arm64 sideload build — v0.9

`Tools/Package-Android.ps1` packages a single-development-APK for Android arm64, using Unreal Engine 5.3, SDK API 33, Build Tools 33.0.1, NDK 25.1.8937393 and JDK 17. It targets API 33 for sideload testing; it is not a Google Play submission. The engine/game content is cooked to Android ETC2. Project and content-only cooker settings are kept in `Game/Config/Android/AndroidEngine.ini` and `Tools/CookProject/Config/Android/AndroidEngine.ini`.

## Build command

Run only after the root task owner has cleared concurrent Unreal compilation/cooking:

```powershell
cd "D:\book of mormon RTS\Revisions\v0.9"
.\Tools\Package-Android.ps1
```

The script first cooks shared Unreal engine content from the content-only `Tools/CookProject/Chronicles.uproject` for `Android_ETC2`, copies the cook into the project cook directory, then builds only the game target for Android arm64, skips a second cook, and stages/packages the APK. For retries after a successful engine cook, `.\Tools\Package-Android.ps1 -ReuseEngineCook` validates the cached engine `.uasset` count and skips the slow initial shader cook. `ANDROID_HOME`, `ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT`, `NDKROOT`, `JAVA_HOME` and Unreal log directories are set only for the script and its child processes, then restored.

The result is `Builds/Android/Chronicles-Android-arm64-ETC2-v0.9.apk`. The build script checks the APK's package name, minimum SDK 26, target SDK 33 and `arm64-v8a` ABI with Android `aapt`; it also fails if UAT produced a separate OBB. Android File Server is disabled for the Android profile. No connected device or emulator is assumed; the script never installs or launches the game on a device.

The APK contains project data inside the package for one-file sideloading. It may be large; Google Play's APK size requirements are not met or targeted by this configuration. Sideload the same package version to every LAN participant. The network protocol remains the UE host-authoritative IP protocol on UDP 7777. Touch controls, performance on real devices and mixed-platform LAN have not been verified by packaging alone.

## Toolchain source

The build reads the root-provided `D:\RTSToolchains\Android\toolchain.json` and validates each pinned SDK component before it starts. It does not run `SetupAndroid.bat`, install Android Studio, persist environment variables, alter firewall settings, or inspect signing credentials. See Epic's [UE 5.3 Android SDK setup](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-set-up-android-sdk-and-ndk-for-your-unreal-engine-development-environment?application_version=5.3) and [Android packaging settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/android-settings-in-the-unreal-engine-project-settings).

Build logs go to `Game/Saved/BuildLogs/`. The initial Android compile exposed `-Werror` failures in `RTSUICommands.cpp` (misleading indentation) and `RTSInputTests.cpp` (misleading indentation and a shadowed field). After those source fixes, `-ReuseEngineCook` compiled and linked the arm64 game, completed Gradle packaging, and passed the script's `aapt` checks.

The final handoff APK is **486,842,611 bytes**, SHA-256 `7E5BFC877CA31B6BFD1A9A2AD32E54AE62A685B287D2ABB4E5A6E57B38B7E0F8`. Static verification passed all 754 staged data-file hashes, all 259 mapped models, all 147 skeletal mesh and skeleton paths, both materials, and the single cooked Pak. See [payload verification](Evidence/v0.9/android/final-handoff/payload-verification.json) and [final artifact ledger](Evidence/v0.9/platform/final-handoff-artifacts.json). Archive verification does not establish device launch, rendering, touch or LAN behavior.

The isolated API 33 ARM64 AVD was attempted with software TCG and rejected by QEMU2 on this x86_64 host before boot. An official API 33 Google APIs x86_64 image/AVD was also tried with TCG; QEMU entered its main loop, but ADB stayed offline and the guest never booted. The x86_64 image's documented NDK translation support has not been verified at runtime. No APK install, app launch, screen capture, or emulator/device result is claimed. See [AndroidEmulator-v0.9](AndroidEmulator-v0.9.md) for commands and captured output locations.

Android emulators are installed but have not booted the game. There is no physical Android device install or launch evidence, and rendering, touch and LAN remain untested on hardware. The APK is a Development sideload build, not a Google Play release artifact.
