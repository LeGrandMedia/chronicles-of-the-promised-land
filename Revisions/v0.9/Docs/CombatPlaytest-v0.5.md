# v0.5 combat playtest

This report summarizes automated native-simulation playtests from [`combat-v05-tests.json`](Evidence/v0.5/native/combat-v05-tests.json). The run recorded **30 assertions passed, 0 failed**, and **12 combat engagements**. It also separately verified the Laban concubine effects described below.

The engagements used 0.1-second simulation ticks on clear terrain, with armies starting 8 metres apart. All were Age III with initial religious stat 20, no upgrades, no kiting, and no human micromanagement; a match stopped at 240 seconds if unresolved. “Cost sum” adds Food, Timber, and Precious Things as plain numbers. It is only an arithmetic proxy, not an equivalence between resource values. Survivors are listed as side A / side B.

| Engagement | Armies (A vs B) | Time | Winner | Survivors | Cost sum (A / B) |
|---|---|---:|---|---:|---:|
| Melee vs bows, equal units | 6 Lamanite Cimeter Bearers vs 6 Lamanite Bowmen | 7.6 s | A | 6 / 0 | 300 / 336 |
| Melee vs bows, reversed-side fixture | 6 Lamanite Cimeter Bearers vs 6 Lamanite Bowmen | 7.6 s | A | 6 / 0 | 300 / 336 |
| Bows vs religion | 4 Lamanite Bowmen vs 4 Priestcrafters | 7.6 s | A | 4 / 0 | 224 / 364 |
| Melee vs religion | 4 Lamanite Cimeter Bearers vs 4 Priestcrafters | 8.8 s | A | 4 / 0 | 200 / 364 |
| Raiders vs line infantry | 6 Gadianton Band of Kishkumen vs 6 Lamanite Cimeter Bearers | 5.5 s | B | 0 / 6 | 390 / 300 |
| Concubines vs melee | 4 Laban Drunken Concubines vs 4 Lamanite Cimeter Bearers | 9.8 s | B | 0 / 4 | 600 / 200 |
| Elephants vs melee | 2 Coriantumr Elephants vs 12 Lamanite Cimeter Bearers | 21.9 s | B | 0 / 10 | 900 / 600 |
| Elite guard vs melee | 4 Laban One of the Fifty vs 12 Lamanite Cimeter Bearers | 14.6 s | B | 0 / 9 | 840 / 600 |
| Nephite vs Lamanite, equal units | 8 Nephite Cimeter Bearers vs 8 Lamanite Cimeter Bearers | 10.2 s | A | 6 / 0 | 760 / 400 |
| Nephite vs Lamanite, rough equal cost | 6 Nephite Cimeter Bearers vs 11 Lamanite Cimeter Bearers | 6.2 s | B | 0 / 11 | 570 / 550 |
| Shiz vs Coriantumr, equal infantry | 8 Shiz Cimeter Bearers vs 8 Coriantumr Cimeter Bearers | 10.1 s | B | 0 / 6 | 600 / 600 |
| Elephant vs watchtower | 1 Coriantumr Elephant vs 1 Lamanite Watchtower | 145.6 s | B | 0 / 1 | 450 / 140 |

In these fixtures, melee strongly beat unmicroed bows. The equal-unit Priestcrafter engagements ended before any conversions completed: incoming damage interrupts conversion and causes a recovery period. The results therefore describe combat under those conditions, not the value religious units might provide when they can complete conversions.

The Lamanite force won the roughly equal arithmetic-cost Cimeter Bearer matchup. The Nephites won the equal-unit matchup, despite their higher combined resource sum. These two observations do not establish a general faction balance result. The two repeated melee-versus-bow fixtures produced the same result.

Elephants lost both when fighting a larger melee group and when attacking a single Watchtower. That raises a siege-strength question for further playtesting; it is not, by itself, proof that siege is underpowered. Elite guards and concubines also lost their recorded matchups against Cimeter Bearers; religious units and concubines can be disrupted before completing their conversion or seduction channels.

Combat inspection found that automatic targeting excluded zero-attack religious units. That bug is fixed: the native assertions verified ranged units automatically acquire visible enemy religious units, and verified damage interruption and its three-second conversion recovery. Laban-specific assertions verified +1 concubine armor, an age-choice bonus for concubines rather than military units, the nearby Laban speed bonus, the expanded 7-metre aura, and loss of that expanded range when Laban moves away. In the combined melee test, those effects reduced the concubine seduction channel to 5.82 seconds. These are isolated implementation checks, not a broad combat-balance finding.

All outcomes here are observations from the stated scripted fixtures. They are not a proof of balance across maps, unit mixes, upgrades, resource gathering, or human play.
