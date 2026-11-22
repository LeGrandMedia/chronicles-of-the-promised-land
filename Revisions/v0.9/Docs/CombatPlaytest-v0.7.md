# v0.7 Combat Playtest

The v0.7 native combat probe recorded 30 assertions passed and 0 failed across 12 scripted engagements. Separate v0.7 checks recorded 63 religious assertions and 46 core assertions passed, with no failures. The combat fixtures use the current simulation code and are recorded in [`combat-v05-tests.json`](Evidence/v0.7/native/combat-v05-tests.json); the supporting reports are [`religion-v07-tests.json`](Evidence/v0.7/native/religion-v07-tests.json) and [`core-v07-tests.json`](Evidence/v0.7/native/core-v07-tests.json).

Each engagement ran on clear ground with 0.1-second simulation ticks, an 8m starting gap, Age III, initial religious stat 20, no upgrades or player micromanagement, and a 240-second limit. The harness directly spawned units and advanced the normal simulation; it did not use test-side damage or steering. “Cost” is the sum of Food, Timber, and Precious Things after faction pricing. That raw sum helps compare the particular fixtures, but treats each resource as equally valuable and is not a full economic comparison. “A/B survivors” and winner refer to the first and second listed sides.

| Engagement | Armies (A vs B) | Time | Survivors A/B | Cost A/B | Winner |
|---|---|---:|---:|---:|---|
| Melee vs bows | 6 Lamanite Cimeter Bearers vs 6 Lamanite Bowmen | 7.6s | 6 / 0 | 300 / 336 | A |
| Melee vs bows, sides reversed | 6 Lamanite Cimeter Bearers vs 6 Lamanite Bowmen | 7.6s | 6 / 0 | 300 / 336 | A |
| Bows vs religious units | 4 Lamanite Bowmen vs 4 Priestcrafters | 7.6s | 4 / 0 | 224 / 364 | A |
| Melee vs religious units | 4 Lamanite Cimeter Bearers vs 4 Priestcrafters | 8.8s | 4 / 0 | 200 / 364 | A |
| Raiders vs line infantry | 6 Gadianton Robbers vs 6 Lamanite Cimeter Bearers | 5.5s | 0 / 6 | 390 / 300 | B |
| Concubines vs melee | 4 Laban Drunken Concubines vs 4 Lamanite Cimeter Bearers | 9.8s | 0 / 4 | 600 / 200 | B |
| Elephants vs melee | 2 Coriantumr Elephants vs 12 Lamanite Cimeter Bearers | 20.7s | 0 / 10 | 900 / 600 | B |
| Elite guard vs melee | 4 Laban One of the Fifty vs 12 Lamanite Cimeter Bearers | 14.6s | 0 / 9 | 840 / 600 | B |
| Nephites vs Lamanites, equal units | 8 Nephite Cimeter Bearers vs 8 Lamanite Cimeter Bearers | 10.2s | 6 / 0 | 760 / 400 | A |
| Nephites vs Lamanites, similar raw cost | 6 Nephite Cimeter Bearers vs 11 Lamanite Cimeter Bearers | 6.2s | 0 / 11 | 570 / 550 | B |
| Shiz vs Coriantumr, equal infantry | 8 Shiz Cimeter Bearers vs 8 Coriantumr Cimeter Bearers | 8.6s | 0 / 6 | 600 / 600 | B |
| Elephant vs Watchtower | 1 Coriantumr Elephant vs 1 Lamanite Watchtower | 138.1s | 0 / 1 | 450 / 140 | B |

The two Lamanite line-infantry comparisons show the impact of their low price. Nephite Cimeter Bearers won the equal-count trial, but their total raw cost was 760 against 400. At roughly equal raw cost, 11 Lamanite Cimeter Bearers defeated 6 Nephite Cimeter Bearers without a loss. This is a strong result for the cheap Lamanite line in these conditions; it does not establish how either faction performs with upgrades, other units, or human control.

Melee defeated bows in both starting-side arrangements. In the equal-count religious-unit trials, Bowmen and Cimeter Bearers defeated Priestcrafters before any conversion completed. The combat checks also verify that damage interrupts conversion and leaves no cooldown, so exposed religious units can be stopped before their channel finishes. Four Concubines likewise lost to four Cimeter Bearers; their Seduce channel is vulnerable when enemies can reach them. These outcomes measure unprotected, unmicroed engagements rather than the value of religious support in a larger army.

The Elephant trials are probes of combat stats, not valid roster openings. The legacy harness resets every player to Age III and directly spawns its units, bypassing the production queue and age gates; Elephants now require Age IV. The Elephant-versus-Watchtower case also directly spawns a Lamanite Watchtower, which is no longer in the Lamanite build roster. It therefore does not represent a matchup that a Lamanite player can currently field. In the recorded Elephant-versus-infantry probe, 12 Cimeter Bearers defeated two Elephants; in the tower probe, the Watchtower survived with 146 HP after 138.1 seconds. The Cement Tower is a separate Jaredite structure and is covered by targeted trebuchet assertions in the religious and core reports.

These results surfaced balance questions but made no automatic balance changes. The script covers a small set of static formations on clear terrain; it does not measure build orders, economy, upgrades, maps, or live player tactics. Final build and multiplayer verification are tracked separately, and this report does not claim a physical multi-PC LAN test.
