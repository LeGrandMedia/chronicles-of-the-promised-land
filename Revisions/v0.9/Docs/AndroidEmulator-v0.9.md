# Isolated Android Emulator readiness — v0.9

**Best-suited / actual model:** GPT-6 Luna / GPT-6 Luna. Prepared isolated API 33 ARM64 and x86_64 AVDs and attempted both under software TCG. Neither reached Android boot completion; the APK was not installed.

## Host and compatibility finding

The development host is Windows 11 Home x64, Ryzen 5 5600X, with firmware virtualization disabled. The Android SDK is pinned at `D:\RTSToolchains\Android` and uses the project JDK 17. Read-only SDK package metadata showed emulator 37.3.10 and the API 33 Google APIs `arm64-v8a` image revision 17 are available from Google's official SDK repository. `sdkmanager` installed those official packages successfully, and `avdmanager` created the AVD. The image declares `SystemImage.Abi=arm64-v8a`; it is a true ARM64 guest and compatible with the APK's ABI, not an x86 image relying on native ARM library translation.

Google's current emulator documentation says ARM images on Intel/AMD hosts cannot use the listed VM acceleration. The documented `-accel off` software mode is unsupported and very slow, and requires `ANDROID_I_WANT_MY_TCG=yes`. The first actual API 33 ARM64 start was rejected by QEMU2 before guest boot. The official API 33 Google APIs x86_64 fallback passed host-architecture validation and entered QEMU, but the guest stayed ADB-offline for more than three minutes under TCG. QEMU then showed a stable near-idle CPU count and its threads were suspended/waiting; the isolated process was stopped. No Android boot, ARM64 translation, APK install, app startup, rendering, touch, or gameplay pass is claimed.

The `-no-accel` option is deprecated in current command-line docs and described for x86/x86_64 images. Use `-accel off` plus the explicit TCG opt-in for this ARM64 guest. No BIOS/hypervisor setting, Windows feature, security setting, or account was changed.

## Installed packages and AVD

- SDK Emulator: `D:\RTSToolchains\Android\sdk\emulator`, revision 37.3.10.
- System image: `system-images;android-33;google_apis;arm64-v8a`, revision 17 (API level 33, Google APIs; no Play Store image or Google account flow).
- AVD: `Chronicles_v09_ARM64_TCG`, Pixel 2 hardware profile, under `D:\RTSTestVM\v0.9\Android\avd\Chronicles_v09_ARM64_TCG.avd`.
- AVD settings: true `arm64-v8a` image; 2 virtual cores; 6 GiB guest RAM; 8 GiB data partition; 720×1280 guest display; SwiftShader software graphics; audio input and cameras disabled.
- Official fallback image installed: `system-images;android-33;google_apis;x86_64`, revision 17. Dedicated AVD: `Chronicles_v09_X86_64_TCG` under the same isolated AVD directory, Pixel 2 profile, 2 virtual cores, 4 GiB guest RAM, 720×1280 display, no audio input/output or cameras. It was attempted with `-accel off`; ARM64 bridge support was not verified because Android never came online.
- `ANDROID_AVD_HOME` and `ANDROID_USER_HOME` are set to dedicated directories under `D:\RTSTestVM\v0.9\Android`; no user-profile AVD was created or modified.
- Setup log: `D:\RTSTestVM\v0.9\Android\SetupAndroidEmulator-v09.log`. Preparation command: `Tools/SetupAndroidEmulator-v09.ps1`. It installs only the official emulator and API 33 arm64 system image, creates the named AVD, and does not start it. It refuses to overwrite an existing AVD.

## Actual startup attempts

After Root cleared the installer stage, the ARM64 attempt used this command (environment variables were scoped to the PowerShell process):

```powershell
$sdk = 'D:\RTSToolchains\Android\sdk'
$env:ANDROID_HOME = $sdk
$env:ANDROID_SDK_ROOT = $sdk
$env:ANDROID_AVD_HOME = 'D:\RTSTestVM\v0.9\Android\avd'
$env:ANDROID_USER_HOME = 'D:\RTSTestVM\v0.9\Android\user-home'
$env:ANDROID_I_WANT_MY_TCG = 'yes'
& "$sdk\emulator\emulator.exe" -avd Chronicles_v09_ARM64_TCG -accel off -gpu swiftshader -no-window -no-snapshot -no-boot-anim -port 5570 -verbose
```

It exited immediately with `FATAL | Avd's CPU Architecture 'arm64' is not supported by the QEMU2 emulator on x86_64 host. System image must match the host architecture.` Captured output: `D:\RTSTestVM\v0.9\Android\arm64-tcg.stdout.log`; command record: `arm64-tcg-command.txt`.

The fallback AVD was launched first with `-avd Chronicles_v09_X86_64_TCG -accel off -gpu swiftshader -no-window -no-snapshot -no-boot-anim -port 5570 -verbose`, then with `-gpu swiftshader_indirect -no-window -no-snapshot -no-boot-anim -port 5570 -verbose`. The first command exited before a guest became available. The second reached `Starting QEMU main loop`; ADB exposed `emulator-5570` as `offline` for more than three minutes. A repeat on port 5572 with `-gpu off -show-kernel` also remained offline. Both running QEMU instances were stopped after confirming no guest boot. The per-process startup output from the first attempt is in `D:\RTSTestVM\v0.9\Android\x86_64-tcg.stdout.log` and `x86_64-tcg.stderr.log`. The port-5570 repeat output was captured in exec session 24127 and the port-5572 repeat in session 53952; neither reached the guest kernel/ADB state.

The Android Developers managed-device reference documents that Google APIs system images support NDK translation, allowing ARM64 APKs on x86_64, but this capability has not been verified in a live guest here. Do not install the APK until a booted fallback reports an ARM64 ABI/translation library at runtime. Current next supported evidence route: run the API 33 arm64 APK on a physical ARM64 Android device, or use an x86_64 host with virtualization enabled and a Google APIs x86_64 emulator, then confirm translation runtime before installing. The current host's firmware virtualization remains disabled; no firmware, security, or Windows hypervisor settings were changed.

## APK now staged for later testing

Root's additional November 22 diagnostics also failed before guest startup. A headless `-gpu swiftshader -feature -Vulkan` run exposed a loopback QMP socket but did not send a QMP greeting or kernel output; ADB remained offline, with about 0.44 CPU seconds and suspended/waiting threads observed in the emulator process. A separate hidden Qt frontend attempt logged a missing `opengl32sw` software library and fallback to system OpenGL, then made no observable guest progress. These observations do not establish the cause. Both task-owned attempts were stopped, leaving the other pre-existing emulator entry untouched. Commands and logs are retained under `D:\RTSTestVM\v0.9\Android\qmp-gles.*`, `qmp-qt.*`, and `qmp-qt-retry.*`. Disabling Vulkan followed the [official emulator troubleshooting option](https://developer.android.com/studio/run/emulator-troubleshooting); it did not resolve this host's startup failure.

The November 22 Android package completed at 486,632,723 bytes, SHA-256 `7F6401F0B8E7C5EAD0E44F8F520A699241A75C95E6DEB58DF471DC070244D3B2`. Root ran the enhanced static verifier: all 754 data file hashes and 259 model mappings matched; all 147 skeletal mesh and 147 skeleton cooked paths plus both faceted materials were present. See [Android-v0.9](Android-v0.9.md) and [payload verification report](Evidence/v0.9/android/payload-verification.json). No guest boot, APK install, or device launch succeeded.

## Official references

- [Android Emulator command line](https://developer.android.com/studio/run/emulator-commandline): current software `-accel off` mode and TCG opt-in; `-no-accel` is deprecated.
- [ManagedVirtualDevice reference](https://developer.android.com/reference/tools/gradle-api/9.1/com/android/build/api/dsl/ManagedVirtualDevice): Google APIs images support NDK translation for ARM64 APKs on x86_64. This is documentation evidence only; runtime bridge acceptance still needs a booted guest.
- [Configure Emulator hardware acceleration](https://developer.android.com/studio/run/emulator-acceleration): ARM system images cannot use the documented VM acceleration on Intel/AMD x86 hosts; `swiftshader` is a software graphics mode.
- [Run apps on the Android Emulator](https://developer.android.com/studio/run/emulator): Windows host support and resource requirements.
- [Android SDK Manager](https://developer.android.com/tools/sdkmanager): package install syntax and SDK repository workflow. `sdkmanager --list` showed the available official package IDs/revisions recorded above.
- [64-bit ABI guidance](https://developer.android.com/games/optimize/64-bit): test native packages with an ABI-matched image/device.
