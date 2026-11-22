# Chronicles of the Promised Land — v0.9 handoff

Prepared November 22, 2026 after the user ended the open-ended development goal and requested final cleanup and deliverables. This is a playable development handoff, not a claim that every original objective is complete. v0.8 and the user's other playtests remain separate.

## Files to use

| File | Purpose |
|---|---|
| `Chronicles-Setup-v0.9.0-Windows-x64.exe` | Windows x64 installer, including Epic's runtime prerequisite installer. |
| `Chronicles-v0.9-Windows-Portable.zip` | Extract the entire archive; run `Chronicles-v0.9/Chronicles.exe`. |
| `Chronicles-v0.9-Android-arm64.apk` | Native Android arm64 development sideload package; device execution remains unverified. |
| `Chronicles-v0.9-Source-and-Obsidian-Vault.zip` | Rebuildable project source, game data/art, tools, evidence, documentation, archived experiments, and the canonical design vault. |
| `DELIVERABLES.json` / `SHA256SUMS.txt` | Byte counts, SHA-256 identities and archive verification results. |

The source ZIP has two main folders: `Chronicles-v0.9` and `Obsidian-Vault`. Start development with the project README, AGENTS.md and Docs/Handoff-v0.9.md. Open `Obsidian-Vault/00 Navigation/Start Here.md` for current design intent. It excludes installed toolchains, build caches, local save/config state and Obsidian application settings. Unreal Engine is not bundled as a development tool. The project contains the cooked-asset source needed by its packaging workflow.

## Included work

- Six civilizations: Nephites, Lamanites, Gadianton Robbers, Coriantumr Jaredites, Shiz Jaredites, and the House of Laban.
- Fifteen maps, including every requested location plus Ripliancum, the Valley of Lemuel and the Great and Spacious Courtyard. Initial workers have equal nearest-resource distances across the four starts on each map.
- A low-poly 3D world with 259 mapped models and 147 skeletal unit variants. Icons and other UI art remain 2D. Units have programmed motion, combat and death presentation; buildings have destruction presentation.
- Local bots with three difficulties and host-authoritative LAN play. Gadianton bots retain the opportunistic policy that brings stolen cargo home.
- Rotated-camera mesh picking and drag selection, configurable shortcuts, idle-worker controls, training queues, and separate Seat rally controls.
- The final HUD pass increases label/caption readability at 1600x900 and 888x500, fixes the Liahona footer, spells out the Period key, and removes a repeated wall-placement prompt.
- Shorter unit/action descriptions and the retained first-pass civilization tuning. Names, catalog rules and model mappings remain centralized and editable.

## Verification boundary

The exact final executable, installer and APK are recorded in `Docs/Evidence/v0.9/platform/final-handoff-artifacts.json`. Final validation is recorded in `Docs/Evidence/v0.9/maps/final-handoff`, with the LAN run in `Docs/Evidence/v0.9/network/final-handoff`. The final Windows executable is `7F30C0F4543A16277C60D389162F7A609B0E79A5AB3494C7BB77F5E8F4BAB9D4`. It passed 4,234 map checks, all 19 native reports, 46 gate-navigation cases, 57 bot assertions and 17 entity-visibility checks. Scene tests passed 86 cases and input tests passed 67 cases at each of 1600x900 and 888x500. Windows touch preview passed 26 cases. The mixed host/client/bot Narrow Neck LAN run passed on the same computer. Android verified all 754 payload hashes, 259 model mappings and 147 mesh/skeleton pairs. These results do not establish physical-network, mobile-device or clean-install behavior.

Windows installation has been compiled but not successfully tested on a clean Windows machine. Android packaging and its payload are statically verified; no Android device or emulator has launched the game. LAN checks use separate Windows processes on one computer, not a physical network or mixed devices. Touch checks use a Windows preview. macOS and iPhone builds are not supplied; preparation/build scripts do not substitute for those artifacts. External Gemini/Claude playtesting was not completed.

## Known unfinished work

Large units can visually clip the existing gate art even though the established navigation passes its fixtures. The new coordinated gate-art/navigation experiment is archived under `Docs/Experiments/GateClearance-20261122`, outside compiled source. Its last full rendered replay failed six of 90 trajectories, so it is not active in this handoff. The next developer should use animated body envelopes, validate both pier-name variants in adjacent gates, and rerun blocked/offset/reverse paths before adopting it.

Gadianton remains weak in the targeted competitive bot samples. The combat-first experiment lost all six matches and brought Seat loss earlier in five; it was rejected. The surplus-ore/food-assignment trial also lost all six, with three earlier and three later losses than its matched control. Neither changes normal bot behavior. Their opt-in diagnostics, raw results and analysis are preserved for further balancing. The new-map and earlier policy trials are limited bot evidence, not a human balance verdict.

Other outstanding checks are real-device performance and touch, clean-machine install/upgrade/uninstall, physical and cross-platform LAN, naval bot strategy, Apple builds, and human/external review. Original-map rock formations and some small secondary queue captions would benefit from a later visual pass.

## Run the working copy

```powershell
cd "D:\book of mormon RTS\Revisions\v0.9"
.\Builds\Windows\Chronicles.exe
```

Use the exact same handoff package for every LAN player. See `PLAYING.md` beside the delivered files, or `Docs/PlayerQuickStart-v0.9.md` in the source project.
