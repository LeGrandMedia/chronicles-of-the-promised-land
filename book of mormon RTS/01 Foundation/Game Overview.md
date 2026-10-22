# Game Overview

Top-down RTS: scout, gather or steal, build, advance, fight/convert, destroy headquarters. Six asymmetric factions, fixed teams, fog/LOS, finite natural deposits, renewable economies, queues, land/water maps. The v0.8 implementation passed its native, rendered-input, and four-process localhost checks; see [[Validation Report]].

## Match contract

- Four ages; advancing selects a faction hero and persistent perk. Heroes may be retrained after death; another HQ must not expose duplicate completed age choices. Nephites/Lamanites have three choices per advance; other choices follow the faction catalog.
- Current standard start: HQ, six workers, 300 Food / 300 Timber / 200 Precious Things, capacity 15, religious stat 20. Jaredites also get a Curelom and seeded cement HQ. Lobby presets can change resources.
- Population maximum 200/player; completed HQ +15, housing +10. Capacity loss does not kill living units; reject invalid reservations. Beast caps: [[Jaredite Cement Construction]].
- An owner without a completed HQ loses after 60s. A replacement foundation worked within 15s pauses the countdown; completion resets it. Abandoned foundations cannot stall indefinitely. Resignation/defeat disables commands; surviving teammates continue.
- Host chooses map, teams/readiness, starting resources, speed, natural-resource fruitfulness and fog. Settings changes reset readiness. Target: 1v1/2v2, land/water, viable land-based openings for all factions.

## Interaction contract

Single click selects/activates; drag selects groups; contextual right-click moves, gathers, attacks, converts, builds, repairs or drops off. Shift queues orders/trains five; Ctrl-number assigns a group, Shift-number adds, double-tap number centers it. Portrait Shift-click removes, Ctrl-click isolates. Idle indicators/shortcuts include traders as workers and exclude builder beasts. Shift-wheel rotates the camera and minimap together; held Delete requires 2s.

Build defaults to Q; programmable bindings appear on square icons. All producers support rallies; HQ right-click sets economic rally, Y military rally. Markets set newborn traders' Galavant routes. Show producer-local/global queues, real task state, effective stats/counters/AoE, religious effects/counts, owner income rates and resource-worker counts. Tooltips fit and use relevant faction information; hide research successors until prerequisites pass.

Queue all paid foundations immediately, including wall lines and Shift continuations; show distinct scaffolding. Remove depleted art/collision and linked ore hills. Center pings on targets. Use quiet, distinct assignment cues, restrained income notifications, smaller animated units and faint team-colored range rings. The Seat of the People is the largest standard building; the Great and Spacious Building is larger. All units have death animations and all buildings have destruction animations. Exact bindings: [Controls](../../Revisions/v0.8/Docs/Controls.md).

Entity intent: [[Units]] / [[Structures]]. Shared simulation: [[Combat and Training Rules]].
