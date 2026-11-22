# v0.4 validation

The Windows Development executable built and packaged successfully on October 21, 2026. Source and packaged executable hashes match.

| Check | Result | Evidence |
| --- | --- | --- |
| Native gameplay and customization | 252 passed, 0 failed | [Reports and log](Evidence/v0.4/native/) |
| Real viewport input, 1600×900 | 32 passed | [Report](Evidence/v0.4/1600x900/input-tests.json) |
| Real viewport input, 1280×720 | 32 passed | [Report](Evidence/v0.4/1280x720/input-tests.json) |
| Four multiplayer processes, 800 units | All clients passed | [Network reports](Evidence/v0.4/network/) |
| Earlier playtest builds | All 120 protected files unchanged | [Baseline](ProtectedPlaytestBaseline-v0.4.json) |

The native checks exercise all six factions' three age advancements, duplicate age locks, actual gathering/delivery, ten crowded workers escaping a Flock, construction assistance and beast auras, refunds, independent livestock, raid routes and balanced stock theft, resource diversion, conversion, combat counters, stealth detection, disguised ownership, and carrier boarding/unloading. Asset import and customization checks also pass.

Viewport checks use actual mouse and keyboard events through Unreal's game viewport. They cover single clicks, right-click gathering, drag selection, workforce/idle selection, Shift training, carrier panels and E unloading, two-endpoint walls and Shift chains, Galavant waypoints, age-choice filtering, and hotkey settings. Screenshots at both resolutions were visually reviewed.

The mixed-faction network run used Gadianton, Lamanites, House of Laban, and Coriantumr Jaredites. It verifies orders, construction, training, rally replication, queued movement, 800-unit snapshots, disguised former-owner views, passenger privacy, friendly stealth state, hidden enemy units, Cement redaction, and shared enemy vision. All four processes exited successfully.

Network testing used four separate processes on this computer with localhost UDP port 17889. Physical LAN computers, routers, and firewall configurations were not exercised. The test harness is opt-in and does not run during ordinary play. Placeholder artwork remains intentional.

Reproduce with `Tools/Test-Revision-v04.ps1` and `Tools/Test-LAN-v04.ps1` after building and staging this revision. The launchers use offscreen test windows and only stop their own tracked test processes if a timeout occurs.

The [44-note checklist](RevisionTasks-v0.4.md) retains recommended model labels and distinguishes automated evidence from source review. The machine-readable [validation summary](Evidence/v0.4/validation.json) records the executable hash and preserved-file counts.
