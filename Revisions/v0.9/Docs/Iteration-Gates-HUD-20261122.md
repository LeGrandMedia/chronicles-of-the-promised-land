# Gate, HUD, and combat-opening iteration — 2026-11-22

**Final handoff decision:** HUD improvements are retained. The gate experiment was withdrawn from compiled source and preserved in [the experiment archive](Experiments/GateClearance-20261122/README.md). Bot opening experiments remain opt-in diagnostics. The narrative below records intermediate work and is not the final release status; see [the handoff](Handoff-v0.9.md).

Recommended/actual model for this working record: GPT-6 Luna / GPT-6 Luna. This records the active source iteration, model assignments, prior-turn evidence, limits, and validation plan. It is not a final test report or release-status update. Root owns the current build and evidence coordination; do not start a competing build.

## Prior iteration boundary

Before the changes in this record, Windows candidate `CC8D75CDE64C6AF89877698515480D7853C672A63E667A015C41BDBF051042B1` had passed 4,234 map checks, 19 native reports, 86 scene checks at 1600×900, 67 input checks at both 1600×900 and 888×500, 17 visibility checks, 50 bot assertions, and 26 touch checks. A mixed host/client-plus-bot Narrow Neck loopback run passed. The installer compiled and the APK passed static payload verification; clean-VM installation and real-device launch were not verified. These results predate the source changes below and do not establish their behavior. See the [prior artifact ledger](Evidence/v0.9/platform/opening-raids-final-artifacts.json), [Development status](Development-v0.9.md), and [six-pass status](PassStatus-v0.9.md).

## Current source scope and ownership

### Gate passage, collision, and wall joins

Navigation integration was previously assigned GPT-6.2 Astra, with root integrating and GPT-6.2 Sol reviewing the fixture. The current coordinated gate-art and route work is root plus GPT-6.2 Sol. Changes include per-unit passage-size and gate-opening metadata in `Game/Content/Data/passages.json`, catalog loading, heading-aware collision at the gate opening, portal entry/exit waypoints protected from path smoothing, and removal of the appropriate interior piers where adjacent completed gates join. See [gate traversal background](GateTraversal-v0.9.md), [passage metadata](Experiments/GateClearance-20261122/candidate/Game/Content/Data/passages.json), [navigation](../Game/Source/Chronicles/RTSNavigation.cpp), and [scene geometry](../Game/Source/Chronicles/RTSScene3D.cpp).

The earlier traversal fixture passed 46/46 for its then-current movement implementation. Its visual-clearance note described the previous opening geometry, so neither that score nor those old measurements substitute for the present coordinated route/mesh validation. Staged candidate results and fixture corrections are recorded below; the in-progress connector fix still needs a fresh run.

### HUD readability

Recommended/actual model: GPT-6.2 Sol / GPT-6.2 Sol. The source pass separates resource names from amounts, shows assigned-worker counts, lets the resource cards cycle, labels Help with F1, enforces readable compact text, sizes cards to captions and keys, keeps full production names and shortcuts in tooltips, and adjusts the Liahona footer and viewport-edge instructions. See [HUD readability changes and review guide](HUDReadability-v0.9.md), [top bar](../Game/Source/Chronicles/RTSHUD.cpp), [selection and command cards](../Game/Source/Chronicles/RTSSelectionHUD.cpp), [minimap](../Game/Source/Chronicles/RTSMinimap.cpp), and [route prompts](../Game/Source/Chronicles/RTSRouteHUD.cpp).

Functional checks do not replace native-size screenshot review. The prior Gadianton capture prompted a follow-up review of resource and hotkey scanability, command-caption size, and the Liahona footer; inspect the new candidate captures before treating those visual concerns as resolved. Touch preview is not physical-device usability evidence.

### Gadianton combat-first opening diagnostic

Recommended/actual model: GPT-6.2 Sol / GPT-6.2 Sol. The new `-RTSV09BotGadiantonCombatFirst` option is honored only with `-RTSV09BotBalance` in non-shipping diagnostics. It orders two paid camp fighters before resuming Forest Foraging. The Wilderness Spy can still be produced first and is not counted as one of those two fighters. Normal games retain the foraging-first opening. Four focused fixture assertions cover default priority, Spy handling, paid fighter orders, and resumption/debit/trace of the foraging job. The balance report records `gadianton_opening_policy` and opening-job trace details. See [bot planner](../Game/Source/Chronicles/RTSBot.cpp), [bot fixture](../Game/Source/Chronicles/RTSBotTests.cpp), and [balance probe](../Game/Source/Chronicles/RTSBotBalanceTests.cpp).

The combat-first option is an experiment, not a proposed normal-match change. The 12-match Zarahemla trial is complete: Gadianton lost all six matches in both arms, and its Seat fell earlier in five of six paired games with a 26.6-second median difference. The candidate therefore remains rejected; normal policy stays foraging-first. The detailed matched results and limitations are in [Gadianton raid and opening evidence](GadiantonRaids-v0.9.md). A separate local Food-assignment diagnostic compiled against the same checkpoint and is queued as an independent trial; it must not be conflated with Combat First.

## Intermediate evidence and corrections

The first HUD/gate binary, `5DD0C8FC8851CE4F6C1F92E792E250F332B36EA61504D7038174DA1DA80654AC`, passed 86 scene checks and 67 input checks at both 1600×900 and 888×500, plus 26 touch-preview checks. Root's visual review and the agent's readability review also passed. Reports and captures are under [first candidate evidence](Evidence/v0.9/maps/gates-hud-opening-first/). These results are staged Windows preview evidence, not physical-device usability evidence.

That binary's gate suite passed 87/94. Six failures were invalid fixtures: a test tree overlapped the mover's spawn and blocked the approach. The remaining failure was a real blocked chariot route. The tree fixture was moved clear of the start for the refined run. The same first native run also exposed an old v0.7 economy fixture issue: it placed a vertical wall but left the mover at default heading 0, and checked internal waypoint behavior instead of actual crossing. The fixture was corrected to set the wall-aligned heading and assert bounded crossing.

The refined binary, `CA16F0D9DC2F0D04DB105A2003B569A0457EE9546BAC936666A7F3E7ADE579F8`, passed all 19 native reports and 54 bot assertions. Its gate suite passed 111/118; the seven failures were blocked-approach navigation cases (ordinary wall: Elephant at 0°, 45°, and 90°, plus King's Chariot at 45°; cement wall: Elephant at 0°, 45°, and 90°). See [refined candidate evidence](Evidence/v0.9/maps/gates-hud-refined/).

The refined rendered-mesh probe is not a pass. It stopped at its 180-second limit with an incomplete set of cases. Several stalled Elephant cases showed no mesh intersection in the sampled poses, but the King's Chariot ordinary-wall case 50 recorded 563 intersecting pose samples, first at sample 37 against `wall_batter`; this is a concrete clearance issue to investigate. The suite's incomplete coverage and that observed collision prevent a clearance conclusion. The probe checks sampled poses against wall-part boxes and does not cover interpolation or moving obstacles.

Root is addressing blocked-start portal connectors conservatively, extending the probe to 120 cases including distant-portal attraction and parallel movement, and adding a faster probe that stops on explicit native failures and retargets mid-crossing. That source was compiling in session 36760 at the time of this update; there is no result yet for the follow-up. Keep the failed and incomplete reports as history rather than replacing them.

## Validation plan

Root is compiling the connector/probe follow-up. Once it is ready, use a fresh unique evidence directory; the runner refuses to overwrite existing evidence. This section is a staged working record and does not assert that the iteration is complete.

1. Re-run native, map, bot, gate-traversal, and visibility suites with `Tools/Test-MapsRevision-v09.ps1`. Include `gates` before `gatevisual`, since the visual probe consumes the latest saved gate trajectories.
2. Re-run `scene`, `input`, and `captures` at 1600×900 and 888×500; run `touch` with the Windows touch preview. Record the executable hash with each report.
3. Re-run gate traversal and mesh checks for Elephants and King's Chariots against ordinary/cement gates, multiple headings, offset and blocked approaches, and merged adjacent piers. Confirm body geometry does not intersect wall parts; screenshots sample crossings and do not cover every frame.
4. Keep the prior HUD visual review in view and inspect refreshed captures for resource names/counts, devotion, Help/F1, shortcut alignment, command and production labels, route prompts, and the full Liahona footer. Include the Jaredite Cement/Glowing Stones state and touch-preview layouts.
5. Run the local Food-assignment diagnostic independently from the rejected Combat First trial. Retain JSON/logs, verify the recorded policy value, and compare matched jobs, resource pulses, and outcomes before drawing a result.

## Known limits

- Passing spawned-world movement assertions do not establish rendered clearance, construction placement, arbitrary maze routing, or mobile performance.
- HUD and gate visual review on Windows does not establish readability or touch behavior on physical devices.
- Normal-bot diagnostics do not establish human competitive balance; the combat-first option remains opt-in and needs matched results.
- Earlier CC8 installer/APK evidence remains platform-limited: no clean-VM install, Android device launch, Apple artifact, physical-network LAN, or external-AI review is established by this iteration note.
- Update this working record with measured outcomes only after root supplies the completed run reports. Current package hashes and release status remain in the root-owned status/ledger docs.