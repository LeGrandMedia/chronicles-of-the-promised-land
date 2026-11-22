# Entity visibility in v0.9

GPT-6.2 Sol was recommended and used for the visibility fix and regression fixture.

An observer at (40, 40) has a clear line of sight to a Storehouse of Provisions at (48, 40). The Storehouse itself interrupts the ray to the terrain cell center at (48.75, 41.25). The old snapshot and bot filter required both rays and omitted the Storehouse from their live entity lists. The same sampling issue could hide targets whose footprint reaches into the observer’s sight range while the terrain cell center lies beyond it. [Entity filter](../Game/Source/Chronicles/RTSWorldSnapshot.cpp) · [Exact LOS and detection](../Game/Source/Chronicles/RTSNavigation.cpp)

The server marks a 16 × 16 grid with the bounds of each eligible observer’s effective sight plus the largest catalog target radius. It checks exact range, line of sight and detection for targets in those cells. The range grid stays on the server. Player snapshots and bot decisions use the same entity filter. The 64 × 64 terrain fog still uses rays to cell centers, so the ground behind an observed building can remain dark. Rescouting the terrain cell can clear stale building memory. Snapshots keep the last observed facts until then. [Snapshot and memory](../Game/Source/Chronicles/RTSWorldSnapshot.cpp) · [Bot planner](../Game/Source/Chronicles/RTSBot.cpp)

## Verification

Run the Windows game with `-RTSV09EntityVisibilityTest -nullrhi -nosound -unattended`. The fixture writes `Saved/Tests/v09-entity-visibility.json`. It checks the rotated off-grid Storehouse through the delivered player snapshot, successful and rejected target orders, concealment and spy detection, independent wall and rock occlusion, large footprints at a coarse-cell boundary, private cargo and passenger identity, and building memory through hidden destruction and rescouting. [Fixture](../Game/Source/Chronicles/RTSEntityVisibilityTests.cpp)

The current Win64 Development executable (`CC8D75CD…`) passed all 17 checks, including exact visibility parity and rejecting all 512 distant targets in the 576-target fixture. [Current report](Evidence/v0.9/maps/opening-raids-final/visibility-1600x900/v09-entity-visibility.json) · [Current package](Evidence/v0.9/platform/opening-raids-final-artifacts.json)

The preceding `CFEC2FF3…` package also passed all 17 checks. In that performance fixture, 64 observers faced 576 enemy targets. Building terrain fog and the range grid together took 0.075 ms. Evaluating the targets took 0.055 ms with the range grid and 5.262 ms in the unfiltered exact-only reference pass. These are timings for one synthetic layout, not a speedup measurement against the former terrain-cell implementation or a live match. Dense, overlapping sight ranges can still require exact checks for many targets. [Earlier Windows report](Evidence/v0.9/maps/opening-visibility-raids/visibility-1600x900/v09-entity-visibility.json)
