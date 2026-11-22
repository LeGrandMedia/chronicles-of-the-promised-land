# v0.7 validation

The packaged Windows executable passed the checks below on 2026-10-21. All evidence was produced after this executable was built, and staged data matches 485 source files. Earlier playtest copies remain unchanged: 768 protected files matched their original hashes.

## Native simulation

488 counted assertions passed, with zero failures, plus the visual import and mesh-animation suites. Coverage includes economy, religion, lobby authority and readiness, fog memory and privacy, conversion immunity, wall queues and per-segment gates, selection, art replacement, and combat. The Lamanite opening test uses normal commands and simulation time to build the first temple, train a priest, research Order, advance to Age III, research Pride, build with a priest, and repair with a worker.

| Report | Passed | Failed |
|---|---:|---:|
| combat-v05-tests.json | 30 | 0 |
| core-v04-tests.json | 27 | 0 |
| core-v07-tests.json | 46 | 0 |
| customization-tests.json | 74 | 0 |
| economy-v03-tests.json | 26 | 0 |
| economy-v04-tests.json | 33 | 0 |
| economy-v05-tests.json | 31 | 0 |
| rally-tests.json | 6 | 0 |
| religion-v03-tests.json | 43 | 0 |
| religion-v07-tests.json | 63 | 0 |
| scenario-tests.json | 17 | 0 |
| self-tests.json | 26 | 0 |
| v07-economy-tests.json | 36 | 0 |
| v07-selection-tests.json | 30 | 0 |
| visual-tests.json | True | 0 |
| visual-v07-animation-tests.json | 0 | 0 |

## Actual rendered input

The executable received actual viewport mouse and keyboard events at 1600×900 (55 checks) and 1280×720 (55 checks). Both runs passed single-click menus and selection, drag selection, right-click gathering, programmable hotkeys, lobby options, double-tap camera centering, selection portraits, independent rallies, wall chaining, tooltip bounds, and six pairs of rendered action-animation frames. Screenshots and reports are in [viewport evidence](Evidence/v0.7/1280x720/). Selected screenshots were visually inspected at 1280×720.

## Four-process multiplayer

One listen server and three client processes communicated through Unreal networking on localhost, using the same packaged build. All four reports passed ordinary orders, lobby settings and readiness reset, five-second conversion and permanent immunity, simultaneous foundations, eligible per-wall gates and unsupported-faction rejection, owner income rates, public tower statistics, financial privacy, stress population, and match completion. The test uses four factions and adds fixture entities and resources for focused checks; gate research time is shortened after a real accepted command. It is an automated network test, not a human match or a physical multi-computer LAN test. Connection and Windows firewall behavior on another PC still need a real LAN play session.

## Art and combat observations

All 238 unique generated PNGs are present in the package, including 13 new hero/research assets with receipts. Unit motion uses deformation of 2D art, with changed mesh poses and no folded triangles in the sampled checks. It is not a rigged 3D asset system.

[Combat playtest](CombatPlaytest-v0.7.md) records 12 engagements and their limitations. Cheap Lamanite infantry is particularly strong in several tested conditions; exposed conversion units are vulnerable to interruption. No extra balance changes were inferred from those observations.

Machine-readable results: [Validation-v0.7.json](Validation-v0.7.json). Executable SHA-256: `4252ca25a83fdb585456a74a3963683e0633446eec97ef18a9682d59bd92930d`. The portable archive has a separate checksum and CRC report beside the ZIP.
