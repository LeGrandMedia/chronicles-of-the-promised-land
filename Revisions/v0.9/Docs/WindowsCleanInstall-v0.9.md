# Clean Windows installer test for v0.9

Recommended and actual task model is GPT-6.2 Sol. The test runs an offline Windows Server 2025 Evaluation guest on QEMU TCG. The host is Windows 11 Home on a Ryzen 5 5600X with firmware virtualization disabled. TCG executes the guest in software and is appropriate for a prerequisite and startup smoke test.

## Verified inputs

| Input | Verified identity |
|---|---|
| Microsoft evaluation ISO | D:\RTSTestVM\v0.9\ISO\WindowsServer2025_Eval_x64.iso, 8,152,356,864 bytes, SHA-256 7B052573BA7894C9924E3E87BA732CCD354D18CB75A883EFA9B900EA125BFD51 |
| Windows image | Index 2, Windows Server 2025 SERVERSTANDARD, ServerStandardEval, Desktop Experience, x64 build 26100, en-US |
| Windows installer | Builds\Installer\GPU-20261122\Chronicles-Setup-v0.9.0-Windows-x64.exe, 524,097,278 bytes, SHA-256 33E6DC4A81BBD0A1089DE0910A7F29A48F5C67C20D1C0202B934BE66CE360D04 |
| MSYS2 base archive | msys2-base-x86_64-20261120.tar.zst, published SHA-256 ADDE59FCB53760BD0FD2AA3C3E8DA516F03159B771DC62B77C340E4AC187FCDD, detached signature verified against primary fingerprint 0EBF782C5D53F7E5FB02A66746BD761F7A49B0EC |
| QEMU | UCRT64 package 11.1.3-2, binary and qemu-img both 11.1.3, TCG and pc-i440fx-11.1 available. Cached package SHA-256 F8AA8EE0CA76747E84168A4A90EDD8FF5D0E8051EA978D60468914C340986772. |

The Microsoft download provenance is in D:\RTSTestVM\v0.9\ISO\source.json. The recorded ISO SHA-256 is a local integrity check. A read-only mount and the install.wim catalogue establish which edition the media contains. The [Microsoft Evaluation Center](https://www.microsoft.com/en-us/evalcenter/download-windows-server-2025) lists the evaluation terms.

Tools/SetupQemu-v09.ps1 verifies the checksum and detached signature before extracting into D:\RTSToolchains\MSYS2. The scoped pacman setup completed the UCRT64 installation with signature verification required. Pacman logs and archive signature evidence live in D:\RTSToolchains\Downloads\MSYS2-20261120. The script records installed package and binary versions in chronicles-qemu-install.json inside the toolchain. [MSYS2 archive verification](https://www.msys2.org/docs/installer/) · [QEMU Windows package instructions](https://www.qemu.org/download/) · [MSYS2 QEMU package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-qemu)

## Run the isolated guest

Run from Revisions\v0.9 in Windows PowerShell or PowerShell 7. Start only in the reviewed VM window.

~~~powershell
.\Tools\SetupQemu-v09.ps1
.\Tools\Prepare-WindowsTestVM.ps1 -Mode Inspect
.\Tools\Prepare-WindowsTestVM.ps1 -Mode Prepare
.\Tools\Prepare-WindowsTestVM.ps1 -Mode StartOffline
.\Tools\Prepare-WindowsTestVM.ps1 -Mode Status
.\Tools\Prepare-WindowsTestVM.ps1 -Mode Screenshot
.\Tools\Prepare-WindowsTestVM.ps1 -Mode ReadResults
~~~

Inspect verifies the ISO identity and reports the Desktop Experience image index. Prepare rechecks the ISO and installer hashes, generates the answer file and builds an ISO 9660/Joliet installer media image. It creates a new standalone 100 GiB dynamic QCOW2 and records its configuration under D:\RTSTestVM\v0.9. Existing VM inputs are retained. The harness's optical-image creation was tested by mounting a small image read-only and reading back its answer file.

The first QEMU boot uses the Microsoft ISO once and boots the virtual disk after Setup restarts. The machine has 2 vCPUs, 8 GiB RAM, an emulated VGA and no NIC. QEMU runs hidden with TCG and attaches both ISOs as read-only CD-ROMs. Its only writable disk is the QCOW2. Results use serial and QMP. The host-side QMP endpoint listens on an ephemeral port bound to 127.0.0.1. Status only queries QEMU, while Screenshot captures the current VGA image into the results folder. [QEMU TCG](https://www.qemu.org/docs/master/system/introduction.html) · [QEMU device and character-backend options](https://www.qemu.org/docs/master/system/invocation.html)

The Windows PE answer file sets en-US, accepts the evaluation EULA, selects the verified image index and installs to one active NTFS partition on virtual disk 0. Specialize stages the installer hook by copying it off the test ISO. Any later Administrator password screen or security prompt remains for the operator. The answer file records no credentials, configures no accounts and leaves OOBE security settings at their defaults. A guest waiting at a screen is evidence of its current stage. [Microsoft unattended setup](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/automate-windows-setup) · [Windows Server installation steps](https://learn.microsoft.com/en-us/windows-server/get-started/install-windows-server)

## Smoke evidence and scope

Windows Setup invokes the smoke as LocalSystem when it reaches SetupComplete. The smoke checks the system SID, the verified OS edition and the optical installer hash. It captures x64 Visual C++ runtime registry state, silently installs to C:\RTSTest\Installed, and keeps its logs under C:\RTSTest. The installer has a 60-minute limit for TCG. The installed game starts with NullRHI only after Inno exits successfully. A pass requires the inner packaged executable to exit 0 within ten minutes and its log to confirm engine initialization and loading /Engine/Maps/Entry. [Microsoft SetupComplete behavior](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/add-a-custom-script-to-windows-setup)

The guest sends the result JSON and bounded installer, game and Windows Setup logs as checked serial records. ReadResults verifies both byte count and SHA-256 before writing the allowed evidence files into Results. The extractor passed a synthetic serial round-trip test. QEMU's raw serial log, run metadata, stderr and VGA screenshots are also retained in Results. A terminal RTSV09 FINISHED marker and SystemBootstrapSmoke.json distinguish a completed smoke from pending setup.

The smoke is designed to cover offline prerequisite installation, game-file deployment and headless engine startup under SYSTEM. An interactive Windows client run is needed for a player's install location, desktop and Start Menu shortcuts, interactive elevation, save locations and uninstall. Rendering, GPU performance and LAN behavior have separate game tests.

## First boot observations

The fresh QCOW2 and optical test media were prepared at 07:34 UTC on November 22, 2026. QEMU started offline at 07:36:49 UTC. QMP confirmed the standalone writable QCOW2 and the two read-only optical drives. Windows Server Setup found the disk and installed the image without input. The guest restarted at approximately 08:36 UTC. Windows Setup's `specialize` pass copied the smoke hook into the guest and emitted `RTSV09 SPECIALIZE_HOOK_STAGED` at 09:04:52 UTC. [Run metadata](<D:/RTSTestVM/v0.9/Results/run.json>) · [QMP storage evidence](<D:/RTSTestVM/v0.9/Results/block-devices.json>) · [First-boot installation](<D:/RTSTestVM/v0.9/Results/guest-20261122-090720.png>) · [Guest serial log](<D:/RTSTestVM/v0.9/Results/qemu-serial.log>)

At 09:08:29 UTC Windows Server reached the built-in Administrator password screen before invoking SetupComplete. The password fields remain blank. QMP paused the offline guest at 09:10:00 UTC and confirmed `status: paused` and `running: false`. The staged SetupComplete hook, clean game installation and NullRHI smoke are pending. [Administrator password screen](<D:/RTSTestVM/v0.9/Results/guest-20261122-090829.png>) · [QMP pause receipt](<D:/RTSTestVM/v0.9/Results/qmp-pause.json>)

To resume this live guest when authorized, verify its PID and QMP port from `Results\run.json`. Connect a QMP client to `127.0.0.1` at that port, complete the `qmp_capabilities` handshake, and send `{"execute":"cont"}`. `Prepare-WindowsTestVM.ps1 -Mode Status` should then report `running`. This continues at the same Windows password screen. [QEMU resume command](https://www.qemu.org/docs/master/interop/qemu-qmp-ref.html#command-QMP-cont)


## Final handoff cleanup

On November 22 the user ended the continuous goal and requested final deliverables. Root verified QEMU PID 37760 as the task-owned offline Chronicles guest, confirmed its paused state through QMP, and issued QMP quit. The process exited; its QCOW2 and evidence remain. The old instructions above describe the historical live run, not a currently running guest. No installer smoke passed. Restarting the existing disk requires reviewing the first-boot ISO/boot arguments; do not blindly rerun a fresh-install sequence against that disk. [Cleanup receipt](Evidence/v0.9/platform/final-handoff-cleanup.json).
