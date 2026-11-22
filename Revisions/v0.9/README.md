# Chronicles of the Promised Land — v0.9

A Windows-playable Book of Mormon RTS development build with six civilizations, local bots, 15 maps, a low-poly 3D world, animated units, and host-authoritative LAN multiplayer. The v0.8 playtest is preserved separately.

```powershell
cd "D:\book of mormon RTS\Revisions\v0.9"
.\Builds\Windows\Chronicles.exe
```

For another Windows PC, use the v0.9 installer or extract the Windows portable ZIP and run `Chronicles.exe`. Use the exact same handoff build on every player computer. The Android APK is a development sideload package whose contents were verified; it has not been run on a physical device. Mac and iPhone builds are not available.

Start with [player instructions](Docs/PlayerQuickStart-v0.9.md), [handoff status](Docs/Handoff-v0.9.md), [development status](Docs/Development-v0.9.md), and [map catalog](Docs/Maps-v0.9.md). [AGENTS.md](AGENTS.md) records coding responsibilities and model recommendations.

Developers: source is in `Game/Source/Chronicles`; data, names, icons, models and audio are in `Game/Content/Data`. Build with Unreal Engine 5.3 and the toolchain in [Tools/ToolchainSetup.md](Tools/ToolchainSetup.md). `Tools/Package.ps1` builds Windows; `Tools/Package-Android.ps1` builds Android. Platform documentation lists required SDKs and current verification limits. The source-and-vault ZIP includes the canonical design vault as a separate `Obsidian-Vault` directory; open its `00 Navigation/Start Here.md` first.

The handoff retains the verified gate navigation and artwork. The unfinished enlarged-gate experiment is archived under `Docs/Experiments/GateClearance-20261122`, outside the compiled source. Gadianton balance diagnostics remain opt-in and do not change ordinary matches. Historical v0.8 documents are retained as history; they are not the current v0.9 implementation specification.
