# v0.8 validation

The packaged Windows build passed 693 counted native assertions, visual import/animation suites, 67 rendered-input checks at 1600×900 and 67 at 1280×720, and four-process multiplayer on localhost. Evidence is newer than the executable. All 494 staged data files match source; 551 protected v0.7 files retain their original hashes.

## Native checks

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
| religion-v08-tests.json | 49 | 0 |
| scenario-tests.json | 17 | 0 |
| self-tests.json | 26 | 0 |
| v07-economy-tests.json | 36 | 0 |
| v07-selection-tests.json | 30 | 0 |
| v08-economy-tests.json | 37 | 0 |
| v08-presentation-tests.json | 119 | 0 |
| visual-tests.json | True | 0 |
| visual-v07-animation-tests.json | 0 | 0 |

Coverage includes trader payouts on blocked waypoints, newborn raid routes, real worker-cargo theft and delivery, generalized Hideout boarding, Winery/G&S generation and ages, visible auras, nonlinear Favor HP, conversion hostility/capacity, Priestcrafter occupancy, beast foundations and repairs, gates, Night Raid, and finite death/collapse poses for every unit/building definition.

## Rendered input and multiplayer

The viewport harness sends actual mouse/keyboard events. It covers clicks and drag selection, hotkeys, Roman lock labels, Winery training, normal/far building views, minimap rotation and inverse clicks, Storehouse rally input, Bribe hover, Hideout boarding, and two frames plus expiry of unit/building deaths. Captures are in [viewport evidence](Evidence/v0.8/1280x720/).

One listen server and three clients use the packaged executable and normal RPCs. New probes cover newborn three-point trader routes, economic and raid rallies, stolen worker cargo without damage, Hideout boarding/unloading, visible Bribe channels and stealth recovery, reliable deaths without a prior entity snapshot, hidden-death privacy, aura replication, and owner-only information. Earlier lobby, conversion, gate-cost, stress and match-completion probes also run. The mass-resignation test exposed and verified a fix for reliable-buffer overflow: compact death events now travel in bounded batches, with native checks for serialization, queue bounds and post-victory draining. Fixtures supply resources/entities for focused checks; this is not a human balance match.

Physical multi-computer LAN and Wi-Fi/firewall behavior remain unverified. No host migration, reconnect or saves. Animation uses deformation of 2D sprites and replaceable frame states; the new robber voice is synthesized placeholder audio. The unchanged UE 5.3 Entry map/engine cook was reused; game code and loose data were rebuilt and staged.

## Delivery

238 active image paths are present, including eight new Laban building assets. [Art provenance and full prompts](ArtGeneration-v0.8.json), [task/model labels](RevisionTasks-v0.8.md), and [machine-readable evidence](Validation-v0.8.json).

Executable SHA-256: `9465ae6f16a46e5eddd22e1b93dbc4e3d2bf23bcacdf73df2a5f077b0026b318`. The portable archive has its own checksum/CRC report alongside the ZIP.
