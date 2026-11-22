# Nephite opening and Favor audit

Nephites start with 20 Favor, six laborers, and 15 population capacity. Standard reserves are 300 Food, 300 Timber, and 200 Precious Things. Favor is a separate stat. It modifies the maximum HP of all Nephite units, including workers and support units. Structures keep their ordinary Nephite HP. The simulation retains the current health fraction when maximum HP changes. [Canonical Favor rules](<../../../../../../book of mormon RTS/02 Systems/Favor of the Lord.md>), [runtime HP and starting reserves](../../../../Game/Source/Chronicles/RTSWorld.cpp)

For a Man of the Sword with 110 base HP, the live curve gives 18 HP at Favor 20, 28 at 25, 40 at 30, 71 at 40, 110 at 50, and 1,100 at 75. Before combat, a completed Temple contributes 0.10 Favor per game second and a Teacher contributes 0.06 before applicable hero bonuses. The decay rate is 0.025 plus 0.0004 times the square of Favor above 20. A single Temple and Teacher therefore tend toward about 38.4 Favor if there are no other awards, penalties, or perks. The health curve rewards keeping both alive and using successful conversions. It does not give a fresh army high HP immediately upon construction. [Curve implementation](../../../../Game/Source/Chronicles/RTSTypes.h), [generation and decay](../../../../Game/Source/Chronicles/RTSReligion.cpp)

## Early costs

Costs apply to Nephites at standard game speed. The times for buildings assume one ordinary builder at its base rate. Extra eligible builders can finish construction sooner.

| Item | Available | Cost | Base time in game seconds | Population |
|---|---|---|---:|---:|
| Industrious Laborer | Age I at the Seat | 63 Food | 18 | 1 |
| Tents and Habitations | Age I | 40 Timber | 15 | +10 capacity |
| Fields of Grain | Age I | 60 Timber | 20 | — |
| Hill of Ore | Age I over a deposit | 60 Timber | 20 | — |
| Storehouse of Provisions | Age I | 80 Timber | 25 | — |
| Fortification Age with Nephi | Age I at the completed Seat | 400 Food, 200 Precious Things | 90 | 4 for the granted hero |
| Encampment of War | Age II | 120 Timber | 35 | — |
| Temple of the Land | Age II | 140 Timber, 100 Precious Things | 45 | — |
| Slinger of Stones | Age II at the Encampment | 50 Food, 25 Timber | 22 | 1 |
| Man of the Sword | Age II at the Encampment | 75 Food, 25 Precious Things | 27 | 1 |
| Bowman | Age II at the Encampment | 63 Food, 44 Timber | 29 | 1 |
| Teacher of the Word | Age II at the completed Temple | 88 Food, 125 Precious Things | 42 | 2 |
| House of Curious Workmanship | Age I | 120 Timber, 80 Precious Things | 45 | — |
| Mighty Change of Heart | Age II at the Temple | 100 Food, 150 Precious Things | 45 | — |

The catalog applies Nephite unit prices at 1.25× and times at 1.20×, rounded up. Structures, research, and age choices use their listed prices and durations. The Seat's queue begins the 90-second age advance after earlier jobs clear and reserves the hero's four population slots. Nephi is granted by the chosen advance and gives the Food-gathering perk. [Production contract](<../../../../../../book of mormon RTS/02 Systems/Combat and Training Rules.md>), [catalog](../../../../Game/Content/Data/catalog.json), [price and time implementation](../../../../Game/Source/Chronicles/RTSCatalog.cpp)

## A legal beginner opening

1. Assign the first six laborers across wild Food, ore, and Timber. Three on Food, two on ore, and one on Timber provide a starting split. Add one or two laborers as Food allows. Use the Seat for early drop-offs and move assignments toward the resource the next purchase needs. A worker carries income back before it reaches the bank.
2. Save 400 Food and 200 Precious Things for the Fortification Age choice and leave the Seat free to start it. Maintain a Timber worker and use one Tent to leave space for Nephi, ordinary troops, a Teacher, and a conversion. Do not fill the first 15 slots if the age advance still needs to reserve four.
3. In Age II, start an Encampment and protect the settlement with Nephi and a few inexpensive Slingers. The Tent, first Encampment, and first Temple use 300 Timber in total. Keep gathering Timber for the Slingers and for any Farm or Hill of Ore added during the opening. The Temple and first Teacher also require 225 Precious Things after the age payment. Preserve ore income instead of committing the entire bank to equipment.
4. Build the Temple near the defended settlement, recruit one Teacher, and convert an isolated eligible attacker from behind the fighting line. A Teacher has zero attack. Soften the Heart requires 60 seconds of hostility and a five-second uninterrupted channel in range and line of sight. Reserve two free population slots in addition to the Teacher's own two for the resulting Ammonite. Keep the Ammonite near wounded allies for healing, or send it to gather when the fight ends.
5. Protect Food deliveries and the Temple as the army grows. A successful conversion grants +10 Favor. Every 100 genuinely gathered Food delivered to an owned drop-off grants +1 within the rolling cap of three per minute. Defensive kills near a completed Seat have a separate +1 award and cap. Killing workers or economic buildings costs 8 Favor, and losing a completed Temple to an enemy costs 10. Aim for enemy troops before committing a low-Favor army to a settlement siege.

This budget is legal on standard reserves and keeps the first defense and conversion prerequisites explicit. It is an opening route for practicing the faction's mechanics. The bot trials did not validate it as a winning build order. [Resource deliveries and combat](<../../../../../../book of mormon RTS/02 Systems/Resource System.md>), [conversion and Ammonite contract](<../../../../../../book of mormon RTS/02 Systems/Conversion and Ammonites.md>), [first-contact evidence](README.md)

## Runtime findings

- The Teacher's conversion is innate. Mighty Change of Heart unlocks the Ammonite's Prostrate window of five seconds on a 12-second cooldown. It is unnecessary for the first conversion. An Ammonite is conversion-only, uses two population, can gather at 1.5×, and heals one nearby ally at 12 HP per second when it is not gathering, delivering, or converting. The cap is 12 Ammonites. Their conversion checks include reserved production population at the start and completion of a channel. [Conversion and capacity](../../../../Game/Source/Chronicles/RTSReligion.cpp), [Prostrate activation](../../../../Game/Source/Chronicles/RTSAbilities.cpp), [heal selection and stacking](../../../../Game/Source/Chronicles/RTSSimulation.cpp)
- **Fixed after this audit:** the defense award previously accepted any enemy non-building with positive Attack, including Laborers. It now explicitly excludes workers, keeping the civilian penalty and the separate award for eligible armed combatants. The observed Nephite balance matches recorded zero opposing worker combat losses, so this corrects the rule without explaining those match outcomes. [Defense award](../../../../Game/Source/Chronicles/RTSCombat.cpp)
- **Fixed after this audit:** an explicit conversion order could follow a target's live position out of range before rechecking visibility. Lost sight now cancels the target, channel and pursuit before movement. Visible targets can still be approached, and a returning target requires a fresh channel. Lawyer exposure remains active during the interrupted bribe. These two fixes passed the 57-assertion religion suite in the packaged native replay. [Conversion implementation](../../../../Game/Source/Chronicles/RTSCombat.cpp), [regression results](../native/wall-escape-regression/religion-v08-tests.json)

## Proposed controlled combat fixtures

1. **Durability only.** Give both sides equal numbers of the same Nephite infantry, with identical positions, gear, and orders. Set starting Favor to 20 versus 35, then 20 versus 50. Swap roles and starts. Keep the fight away from a Seat and record initial and ending Favor, max HP, damage, survivors, and time. A same-faction setup isolates the Favor curve while retaining the normal decay and hit rules.
2. **Recruitment on equal reserves.** Give a Nephite and a Lamanite one completed Encampment, the same capacity, Age II, and 780 Food plus 260 Precious Things. Use the legitimate training queue for the same Man of the Sword definition on both teams. A Nephite can buy ten at 75 Food and 25 Precious Things. A Lamanite can buy twenty at 39 Food and 13 Precious Things. No further income is added. Record units delivered at 90, 180, and 300 game seconds and reserves left over. This measures the cost and throughput difference before any combat.
3. **Combine only after reporting the controls.** In a separate short leg, deploy the trained rosters under mirrored combat orders and record starting Favor and Anger, total HP, and casualties. Equal-spend results combine training and durability. Equal-count and same-faction results provide the controls for interpreting that difference.

These are proposed synthetic test fixtures. They would use isolated local setup, the native queue or combat rules after setup, and unchanged catalog and Faith mechanics. No fixture results are claimed here.
