# Gate traversal across the navigation grid

Final handoff: the established gate navigation and original artwork are retained. The enlarged-portal experiment is [archived outside compiled source](Experiments/GateClearance-20261122/README.md). The final binary repeated all 46 movement cases successfully; existing large-unit visual clipping remains unresolved. [Final evidence](Evidence/v0.9/maps/final-handoff/gates-1600x900/gate-traversal-tests.json).

Recommended model: GPT-6.2 Astra for navigation integration; actual implementation and fixture: root integration agent. GPT-6.2 Sol independently reviewed the fixture and diagnosis. This pass changes authoritative movement, not the gate's price, collision radius, ownership rules or artwork.

The first movement audit reproduced five failures among 28 cases. An elephant could fit through the cement gate according to `Walkable`, but the 1 m route grid missed the narrow corridor or rejected its diagonal connection. The elephant then walked around the seven-segment barrier. The continuous center corridor was clear in all five failures. [Initial report](Evidence/v0.9/native/gate-grid-initial/gate-traversal-tests.json) · [Binary identity and exit](Evidence/v0.9/native/gate-grid-initial/run.json)

`Route` now checks a direct path that crosses a completed friendly gate. If that path is obstructed, it tries gate-centered approach, crossing and exit points, ordered by total travel distance. Every segment is checked with the moving unit's real radius through the existing authoritative `Walkable` function at intervals no greater than 20 cm. At most four candidate gates receive this extra check. Routes with more complicated obstacles fall back to the existing grid. Ordinary per-step collision and speed limits still apply.

The expanded audit passed **46/46** on Windows Development executable SHA-256 `9F905030454E430CCC094AC342A7108D1DA773150DFB04D08951313F8C1566FE`. It covers workers and elephants, ordinary and cement walls, 0°/45°/90° headings, two grid alignments, offset approaches, owner/allied passage, enemy exclusion, unfinished gates, unconverted walls and bounded movement. The five reproduced detours changed from 19.64–19.86 m around the wall to 7.75 m through the gate. [Fixed report](Evidence/v0.9/native/gate-grid-fixed/gate-traversal-tests.json) · [Run identity](Evidence/v0.9/native/gate-grid-fixed/run.json)

The same executable passed all 19 native regression reports, all 216 wall-placement/navigation assertions and all 45 bot checks, including the six standard faction openings. [Native run](Evidence/v0.9/native/gate-route-regression/self-tests-run.json) · [Wall run](Evidence/v0.9/native/gate-route-regression/wall-placement-run.json) · [Bot run](Evidence/v0.9/native/gate-route-regression/bots-run.json)

These movement fixtures use spawned completed barriers, not rendered scenes or construction tests. The cement gate's authored opening remains approximately 1.11 m wide, narrower than the elephant's body and 2.10 m collision diameter. Visual clipping therefore remains an art problem. Gate-centered routes involving obstructed approaches may still require the grid fallback; this change does not establish arbitrary maze routing or device performance. The refreshed [Gate-District installer and Android APK](Evidence/v0.9/platform/gate-district-artifacts.json) include this navigation change; fresh installation and actual Android execution remain unverified.

## Visual clearance audit — unimplemented

Read-only measurements, recommended / actual model: GPT-6.2 Sol / GPT-6.2 Sol. This audit does not alter or extend the 46/46 navigation result above. At 0.45 m above ground, the ordinary and cement gate throats measure 2.145 m and 1.110 m. Lowest hanging teeth are at 1.410 m and 1.400 m; lintel undersides are at 1.565 m and 1.555 m.

| Mover | Authored width × height | Radius | Maximum straight crossing offset | Required visual envelope |
|---|---:|---:|---:|---:|
| Elephant | 1.618 × 2.977 m | 1.05 m | ordinary ±1.34 m; cement ±0.22 m | ordinary 4.298 m; cement 2.058 m |
| King's Chariot | 1.910 × 2.729 m | 0.45 m | ordinary ±1.94 m; cement ±0.82 m | ordinary 5.790 m; cement 3.550 m |

Intact adjacent wall openings at the same height are 4.150 m for ordinary masonry, 4.0165 m for palisade (3.962 m near ground), and 2.146 m for cement. Enlarging the arch alone therefore cannot clear every legal offset. These are static geometry measurements, not tests with rendered moving units.

A candidate coordinated art-and-route correction is a 2.30 m ordinary and 2.12 m cement clear opening, 3.30 m overhead clearance, and cement piers integrated into the neighboring wall mass. For large-looking movers, prefer a verified center entry/exit about 3 m beyond the gate, preserve through-path smoothing, and retain the legal fallback. This candidate still permits clipping when an offset fallback is used, and is neither implemented nor approved as sufficient. Full visual agreement may require coordinated changes to neighboring walls and movement paths.

Validate any future change with rendered, animated Elephants and King's Chariots at 0°, 45° and 90°: centered crossings, maximum-offset crossings, and center approaches blocked to force fallback. Compare against intact neighboring-wall geometry and inspect both the body and carried/attached parts through the opening.