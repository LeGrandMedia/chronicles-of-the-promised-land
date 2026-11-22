# Capability report — A005

Observed October 20, 2026 (America/Phoenix). Machine-readable evidence and exact discovered paths: `Evidence/2026-10-20/capabilities.json`. Reproduce discovery with `Tools/Inventory-Capabilities.ps1`; it reads installations and writes only the report.

| Capability | Observation | Limit |
| --- | --- | --- |
| Unreal | Engine Build.version files verify 5.3.2 CL 29314046 at `C:/Program Files/Epic Games/UE5/UE_5.3` and 5.1.0 CL 23058290 at `D:/Unreal Engine 5/fortnite/UE_5.1`; editor executables and build scripts discovered | Neither launched or pinned; compatibility not tested |
| C++ | MSVC compiler executables discovered: 14.44.35207 (2022 BuildTools) and 14.29.30133 (2019 BuildTools) | vswhere reports 2022 installation incomplete/not launchable; 2019 complete. Not a compile result. A006 must select/verify compatible tools |
| Windows SDK | Include directories 10.0.19041.0 and 10.0.26100.0 present | Header discovery does not prove linker/library setup |
| Blender | Steam uninstall registration points to `D:/SteamLibrary/steamapps/common/Blender`; executable file/product metadata reports 5.2 | `--version` invocation returned no captured version text; launch/render/export not verified. File metadata is discovery evidence only |
| Tools | Python 3.14.2, Git 2.40.0.windows.1; dotnet executable available | Blender, Unreal, cl and cmake absent from current command search path |
| CPU/GPU | Registry: Ryzen 5 5600X, six-core CPU; environment reports 12 logical processors. NVIDIA GeForce RTX 4070; registry dedicated memory 12,878,610,432 bytes | No performance test; do not advertise minimum/recommended specs |
| RAM/OS | Physical RAM unknown; Win32 CIM queries denied. OS API reports Windows NT 10.0.26300.0 | Product edition not verified; future benchmark operator must record RAM |
| Disk | Discovery found roughly 1.78 TiB free on C and 644.6 GiB on D | Snapshot only; engine source/build caches can consume substantial space |
| Permissions | Workspace files and Git baseline successfully written. Network restricted; permitted workspace/visualization roots known | Administrator access, installs, firewall edits, marketplace/account permissions, external storage and editor connectors unverified |

No `.uproject`, game source, assets, packages or execution state existed before this session. No `AGENTS.md` found in D:/, workspace root, vault root or recursive workspace file inventory. No existing ancestor repository was found. Unreal/Blender discovery does not substitute for A013/A019 packaged checks. Repair or verify compiler setup as part of A006, then prove a minimal package before gameplay work.
