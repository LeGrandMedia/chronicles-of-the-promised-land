# v0.4 balance and scripture context

Values below describe the v0.4 content specification. Scripture references support the use of names and historical context, not invented statistics or mechanics. Native, viewport, and multiplayer validation is recorded in [Validation v0.4](Validation-v0.4.md).

## Civic resources and faction traits

All factions start at 20 civic-resource points. Per-second decay is `0.025 + 0.0004 × max(0, Favor - 20)^2`. Conversion gives +5 to the converting player and costs the former owner 5 points, or 10 Lasciviousness for a Gadianton victim. A Nephite direct civilian kill costs 8 Favor; Lamanite unit losses add 3 Anger and building losses add 5. Temples produce 0.1 civic resource/s, religious units 0.06/s, and the Gadianton lodge 0.16 Lasciviousness/s until scouted.

| Trait | Effect |
| --- | --- |
| Favor | +0.4% maximum HP |
| Anger | +0.3% gathering and attack rate |
| Lasciviousness | Above 30 activates researched 2×/3× raid multipliers; above 50 activates shared vision; above 70 activates 50% camp diversion of deposits, rising to 100% at 100. |
| Dominion | +0.3% maximum HP and +0.25% damage |
| Drunkenness | Splash = 0.35 × base attack × Drunkenness/100; radius = 1.4 + 0.015 × Drunkenness |

## Combat, conversion, and aura rules

Ranged units deal +50% damage to religious units. Melee units take 25% less damage from ranged attacks. Religious conversion channels take 8 seconds against melee targets and 12 seconds against ranged targets; defensive towers take 12 and 18 seconds respectively. The Great and Spacious Building has an 8m weakening aura. A Drunken Concubine weakens enemies within 5m by reducing movement speed 20%, attack speed 25%, and maximum HP 15%; current HP fraction is preserved.

## Gadianton economy

Bands of Kishkumen use raid move to raid the first eligible enemy economic structure in sight, then return to the nearest friendly drop-off. Raid move does not damage the structure. Storehouse-to-Storehouse orders repeatedly steal resources. Plunder Share and Plunder Hoard can be researched at any current Lasciviousness; their 2×/3× raid-return effects activate only while Lasciviousness remains above 30. Network of Secrets vision sharing activates above 50. Above 70, a stealthy raider camping at an enemy drop-off diverts 50% of deposits to Gadianton; at 100, it diverts all deposits. These are active-effect thresholds, not purchase gates. Signs and Symbols routes enemy-worker deposits to the nearest eligible Gadianton Storehouse.

## Traders, livestock, and buildings

Trader in Precious Things follows its player-set route. Passing within 4m of an own worker grants 2 Precious Things; the cooldown is 30 seconds and shared by all of the owner’s traders. The cooldown prevents overlapping Traders from duplicating a visit reward.

Curelom is an Age-I Seat of the People beast that builds Age-I/II structures and produces 0.4 Timber/s. Cumelom is an Age-II flock beast that builds through Age III and produces 0.3 Precious Things/s. Elephant is an Age-III flock beast that builds every age, carries units, attacks structures, and produces 0.2 Cement/s. Winery produces 0.5 Food/s. Jaredites may build 10 Flocks; other factions may build 5. Each Flock recruits up to 10 sheep, while captured sheep are independent of that recruitment cap.

Passenger capacity: King's Chariot 6 population, Elephant 10, Jaredite Barge 12, Hagoth's Vessel 10, and other boats 10. Carrier population cost is 3 for these units.

## Hero choices and era framing

For Lamanites at Age IV, Tubaloth is the military choice and Aaron is the economy choice.

Each faction offers two choices at each of Ages II, III, and IV. The military choice gives +10% faction military damage; the economy choice gives +10% worker gathering. The selections are a gameplay-era structure and do not assert a literal chronology or alliance. For Laban and some Jaredite/Gadianton choices, the framing is explicitly interpretive.

Primary scripture references for named figures are linked in the catalog source fields; for example [Helaman 2](https://www.churchofjesuschrist.org/study/scriptures/bofm/hel/2?lang=eng), [3 Nephi 3–4](https://www.churchofjesuschrist.org/study/scriptures/bofm/3-ne/3?lang=eng), [Ether 7](https://www.churchofjesuschrist.org/study/scriptures/bofm/ether/7?lang=eng), and [1 Nephi 3–4](https://www.churchofjesuschrist.org/study/scriptures/bofm/1-ne/3?lang=eng).



