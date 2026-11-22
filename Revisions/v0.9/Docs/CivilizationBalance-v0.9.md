# Civilization balance in v0.9

Recommended model and actual model for the tuning, implementation, and review were GPT-6.2 Sol.

## What the earlier matches showed

The 30-game Normal-bot tournament on Zarahemla Valley recorded seven Lamanite wins, six Nephite losses, and ten Gadianton time limits. In eight of those time limits, Gadianton had zero remaining workers and zero fighters, and the attacker still needed to find a concealed Seat. Later detector replays resolved eight games against Gadianton as victories for their opponents. Gadianton's median Age II timing of 262.1 game seconds trailed the other five civilizations by 81.1–110.6 seconds. Their largest observed raid load was 31.4 resources, with raid cargo observed in three matches. [Tournament and detector replay](Evidence/v0.9/balance/README.md)

At first contact in the six examined Nephite losses, Favor was 22.18–25.22 and their live army max HP totaled 92–233 against 685–986.5. Nephite nominal army cost was higher in five legs. All six sides reached first contact before their first Teacher. Earlier Temple construction put a finished Temple at first contact in four of six matches but produced zero Teachers by contact. The emergency Slinger trial fielded more nearby defenders against the House of Laban and still lost all four trial games across the two opponents. The direct change therefore targets durability at the Favor levels these real openings reached. [First-contact observations](Evidence/v0.9/balance/README.md#first-contact-faith-diagnosis) · [Temple trial](Evidence/v0.9/balance/temple-trial-summary.json) · [Slinger residence and losses](Evidence/v0.9/balance/emergency-slinger/local-defense/README.md)

## Live rules

| Civilization | Previous | v0.9 tuning |
|---|---|---|
| Nephites | A 110-base-HP unit had 18 HP at starting Favor 20, 28 at Favor 25, and 1,100 at Favor 75. | The same unit has 55 HP at Favor 20, 66 at Favor 25, and 220 at Favor 75. Favor 50 gives base HP and Favor 100 gives triple base HP. |
| Lamanites | Ordinary units cost 65% and took 60% of listed time before Anger. | Ordinary units cost 75% and take 75% of listed time, rounded up. Anger continues to improve training, gathering, and attack speed. |
| Gadianton Robbers | Permitted manual labor gathered at 35% of the ordinary rate. | Permitted manual labor gathers at 45%. Raid cargo and foreign food access still reward infiltration, alongside a weaker manual economy than the other civilizations. |

- **Nephites.** With Favor at or below 50, interpolate from 1 HP to base HP using `t × (1.4 − 0.4t)`, where `t = Favor / 50`. Above 50, unit max HP increases linearly from base to triple base. Round max HP and preserve the current health fraction. Structures use their ordinary HP. Nephite ordinary units cost 125% and take 120% of listed time, rounded up. [Health calculation](../Game/Source/Chronicles/RTSTypes.h) · [Favor awards and hover](../Game/Source/Chronicles/RTSReligion.cpp) · [Recruitment calculation](../Game/Source/Chronicles/RTSCatalog.cpp)
- **Lamanites.** At base catalog rates, a Cimeter Bearer costs 50 Food and 25 Precious Things and takes 20 seconds. Lamanites pay 38 Food and 19 Precious Things with a 15-second job before Anger, compared with 33 Food and 17 Precious Things and a 12-second job before this pass. The smaller discount limits the recruitment lead while preserving a clear advantage in producing large armies. [Catalog calculation](../Game/Source/Chronicles/RTSCatalog.cpp) · [Authority queue and Anger assertions](../Game/Source/Chronicles/RTSV07EconomyTests.cpp)
- **Gadianton Robbers.** The 10 percentage point increase helps workers fund Age II and replacement troops. The same factor applies to wild or permitted foreign Food, Timber, and Precious Things. The normal availability checks still exclude owned farms, and stored or stolen cargo follows existing drop-off rules. [Gathering rules](../Game/Source/Chronicles/RTSEconomy.cpp)

The default computer policies use the same tuning as human players. Emergency Slinger priority stays restricted to the opt-in balance probe. The House of Laban and both Jaredite factions use their existing faction mechanics, so their head-to-head tests help identify spillover effects from the tuning.

## First paired Windows replay

The same Development executable completed all 24 matches with the previous rules and the first pass, which adjusted Nephite health, Lamanite recruitment and Gadianton gathering. Each arm played two starting-slot assignments per pair on Zarahemla Valley, with the same Normal planners and support rules. All reports passed their simulation validity checks. Game times below are seconds. A time limit indicates an unfinished match at the 12,000-step cap. [Previous rules](Evidence/v0.9/balance/civilization-first-control/) · [First pass](Evidence/v0.9/balance/civilization-first-tuned/)

| Pair | Previous rules | First pass |
|---|---|---|
| Nephites–Lamanites | Lamanites 592.1 and 520.9 | Lamanites 650.2 and 614.8 |
| Nephites–Shiz | Time limit, Shiz 820.5 | Time limit, Shiz 964.0 |
| Nephites–House of Laban | Laban 785.0 and 732.1 | Time limit, Laban 1,066.1 |
| Lamanites–Gadianton Robbers | Lamanites 495.3 and 783.7 | Lamanites 858.5 and 650.5 |
| Lamanites–Coriantumr | Lamanites 813.4 and 744.0 | Two time limits |
| Gadianton Robbers–Shiz | Shiz 833.8 and 797.9 | Shiz 744.0 and 793.7 |

- **Nephites** lost four of six games, down from five under the previous rules. First-contact army HP rose 2.3–3.3 times and reached a median of 48.8% of opposing HP. The previous House of Laban loss that reached the time limit left both Seats alive. Nephites converted six, fielded nine fighters at the limit and recovered to 25 workers. Nephites still fielded less army population at first contact in all six games, had a higher nominal bill for that army in five, and fielded their first recruit at the same times in both arms. That persistent roster gap motivated the second-pass recruitment change. [Lamanite replay](Evidence/v0.9/balance/civilization-first-tuned/map-0/pair-0/) · [Shiz replay](Evidence/v0.9/balance/civilization-first-tuned/map-0/pair-3/) · [Laban replay](Evidence/v0.9/balance/civilization-first-tuned/map-0/pair-4/)
- **Lamanites** won twice against Coriantumr under the previous rules, and both games changed to time limits with both Seats standing. Lamanite peak army population in those isolated tests fell from 47 to 25 and from 44 to 22. Lamanites still won both games against each of the Nephites and Gadianton. [Coriantumr replay](Evidence/v0.9/balance/civilization-first-tuned/map-0/pair-6/)
- **Gadianton** reached Age II 14.4–28.8 game seconds earlier and added one to five peak army population. All four losses repeated. The Seat held 363.2 seconds longer in one Lamanite matchup and fell 4.2–133.2 seconds earlier in the other three. Gadianton raiders acquired zero cargo across these games, so the sample establishes the gathering effect and leaves raid effectiveness open. [Lamanite replay](Evidence/v0.9/balance/civilization-first-tuned/map-0/pair-5/) · [Shiz replay](Evidence/v0.9/balance/civilization-first-tuned/map-0/pair-10/)

The previous-rules arm took 91,190 steps and the first pass took 111,421. Median per-match step medians were 1.64 and 1.73 ms, and median per-match 95th percentiles were 19.04 and 19.57 ms. Three concurrent processes ran each batch. Changes in match length and roster sizes affect the comparison.

## First-pass packaged runtime verification

The first-pass Windows Development executable `B1F5EA28ACBD61972088CB128BB90D28A60793C5C670045DE142411E935DCF2A` exited 0 for the native suite with all 19 reports passing. This included the 18 scenario checks, 68 religion checks for v0.8 mechanics and tuning, and 37 v0.7 economy checks. The public queue and refund assertions verified a 27-second Nephite Man of the Sword job and the full return of its 75 Food and 25 Precious Things after cancellation, alongside the new Lamanite debit and duration. [Run metadata](Evidence/v0.9/maps/maps-balanced/native-1600x900/run.json) · [Scenario report](Evidence/v0.9/maps/maps-balanced/native-1600x900/scenario-tests.json) · [Religion report](Evidence/v0.9/maps/maps-balanced/native-1600x900/religion-v08-tests.json) · [Economy report](Evidence/v0.9/maps/maps-balanced/native-1600x900/v07-economy-tests.json)

That package passed all 45 bot assertions. Each civilization earned deposits, built, researched, recruited, advanced and landed attacks in its opening against a passive settlement. Gadianton reached Age II at 234.1 seconds and won its Land of Many Waters scenario at 812.6. [Bot checks and opening milestones](Evidence/v0.9/maps/maps-balanced/bots-1600x900/v09-bot-tests.json)

The earlier cheaper-recruitment package passed all 19 native reports, including the experiment's 25-second Man of the Sword queue and 68 Food and 23 Precious Things refund. [Trial scenario checks](Evidence/v0.9/maps/maps-final/native-1600x900/scenario-tests.json)

## Nephite recruitment comparison

The same Windows binary replayed six mirrored Nephite games with the first-pass rules and with cheaper and shorter recruitment. The trial dropped Nephite cost and training multipliers from 1.25 and 1.20 to 1.125 and 1.10. That lowered a Man of the Sword from 75 Food, 25 Precious Things, and 27 seconds to 68 Food, 23 Precious Things, and 25 seconds. All six controls reproduced every retained gameplay field from the first-pass tournament after excluding wall and step timings. Both arms used the new Favor curve and the same opponent tuning and map. [First-pass controls](Evidence/v0.9/balance/civilization-second-control/) · [Recruitment trial](Evidence/v0.9/balance/civilization-second-tuned/)

| Nephite opponent | First pass | Cheaper and shorter Nephite recruitment |
|---|---|---|
| Lamanites | Lamanites 650.2 and 614.8 | Lamanites 725.2 and 619.9 |
| Shiz | Time limit with both Seats, Shiz 964.0 | Shiz 995.2, time limit with zero Nephite Seats |
| House of Laban | Time limit with both Seats, Laban 1,066.1 | Laban 1,090.1 and 1,047.8 |

Cheaper recruitment let the Nephite bot field a Teacher in five starts instead of three and raised total conversion count from nine to 15. Eleven of the 15 came in the second Shiz start, where Nephites lost two Seats just before the 1,200-second limit. The first recruit appeared 0.2–2.0 seconds earlier and first-contact army population improved in one of six starts. The outcomes moved from four losses and two time limits with both Seats to five losses and one time limit during defeat grace. The default game uses the first-pass recruitment multipliers, and the probe can replay the cheaper trial. [Recruitment reports and run metadata](Evidence/v0.9/balance/civilization-second-tuned/)

## Matches on the new maps

The B1 first-pass Windows executable ran 12 valid mirrored games across Bountiful, the Narrow Neck of Land, and the Hill Cumorah. Eight ended in victory, three in time limits with both Seats, and one in a time limit while Laban was in defeat grace. Each pair played under the retained civilization tuning. The sample tests Normal land-based AI on three maps. [Raw match reports and timings](Evidence/v0.9/balance/civilization-new-maps-balanced/)

| Map | Pair | Slot assignments, in report order |
|---|---|---|
| Bountiful | Nephites–Lamanites | Time limit with both Seats, Lamanites 646.2 s |
| Bountiful | Gadianton Robbers–House of Laban | Laban 732.9 s and 697.5 s |
| The Narrow Neck of Land | Nephites–House of Laban | Time limit during Laban's defeat grace, Laban 756.8 s |
| The Narrow Neck of Land | Lamanites–Coriantumr | Time limit with both Seats, Coriantumr 1,066.8 s |
| The Hill Cumorah | Nephites–Shiz | Shiz 817.0 s, time limit with both Seats |
| The Hill Cumorah | Gadianton Robbers–Shiz | Shiz 848.9 s and 1,103.9 s |

In the first Narrow Neck leg, Nephites took Laban's Seat at 1,157.1 seconds, converted 11 units and reached the time limit with their own Seat, 28 workers and 18 fighters. They also converted ten in the Bountiful time limit against the Lamanites and eight in the Cumorah time limit against Shiz. Gadianton recorded four losses and one positive raid cargo observation. All four Gadianton starts reached Age II between 234.1 and 246.9 seconds. [Narrow Neck Nephite replay](Evidence/v0.9/balance/civilization-new-maps-balanced/map-8/pair-4/) · [Bountiful Nephite replay](Evidence/v0.9/balance/civilization-new-maps-balanced/map-3/pair-0/) · [Cumorah replay](Evidence/v0.9/balance/civilization-new-maps-balanced/map-10/)

The new-map sample ran 114,704 steps. The medians across per-match step medians and per-match 95th percentiles were 1.48 ms and 13.25 ms. Two processes ran concurrently on each map.

## Gadianton raid diagnosis

The original round robin and the detector replay each observed a raid order in four of ten Gadianton legs and positive cargo in three, all against the Nephites or House of Laban. In the first-pass Lamanite and Shiz sample, one of four Gadianton sides entered the raid order, against Shiz at 479.1 seconds, and all four recorded zero raid cargo. The pre-tuning controls for those four games issued zero raid orders. The native suite separately proves that direct worker raids can transfer real cargo and return it to a friendly drop-off. [Round robin and detector replay](Evidence/v0.9/balance/README.md) · [Tuned Shiz pair](Evidence/v0.9/balance/civilization-first-tuned/map-0/pair-10/) · [Authority economy checks](Evidence/v0.9/maps/maps-balanced/native-1600x900/v08-economy-tests.json)

The earlier planner queued Bands at an idle Storehouse after fielding three military units. The same Storehouse first researches Forest Foraging when the base is safe. A Band takes the first currently visible eligible enemy building in entity order. It uses generic combat orders when a threat enters the base defense radius, including if it was returning with cargo. The authority also supports raiding visible workers carrying resources, raid-move, and setting a Storehouse raid route. Workers can already consider visible foreign farms and flocks through the normal assignment rules. [Planner](../Game/Source/Chronicles/RTSBot.cpp) · [Authority orders](../Game/Source/Chronicles/RTSCommands.cpp) · [Raid channel and drop-off](../Game/Source/Chronicles/RTSEconomy.cpp)

This diagnosis motivated a targeted replay of Storehouse gates, first Band fielding, visible targets, channel interruptions and real cargo delivery, with foreign Food counted separately. That replay and the resulting policy are now documented in [Gadianton raids](GadiantonRaids-v0.9.md). Zarahemla pairs 5 and 10 supply zero-cargo outcomes, and the historical House of Laban pair 11 supplies a successful reference. The retained tuning also produced 18.6 peak cargo against Shiz in the second Cumorah leg after a first raid order at 524.8 seconds. Comparing these starts can establish where the gap occurs across production, target access, the three-second channel, and returning to a drop-off. [Cumorah Gadianton replay](Evidence/v0.9/balance/civilization-new-maps-balanced/map-10/pair-10/)

## Follow-up retained in the current package

The `CC8D75CD…` package retains the civilization multipliers above and adds [equal initial resource access](OpeningFairness-v0.9.md), [correct entity visibility](EntityVisibility-v0.9.md), and [opportunistic bot raids](GadiantonRaids-v0.9.md). A same-binary raid-policy comparison delivered 106.5 resources in seven trips versus zero with the old planner. The final package delivered another 81.0 across six games on three maps. Gadianton still lost all competitive games in those samples. The first-pass tables above are historical measurements, not results for the newer openings or visibility rules.

## Repeatable checks

- Run the Development executable with `-RTSSelfTest -nullrhi -nosound -unattended`. The native suite checks Favor 0, 20, 25, 50, 75, and 100 across workers, fighters, heroes, Ammonites, and ships. It also checks structure HP, preserved health fractions, the Favor hover, the actual Lamanite queue debit and duration, Anger acceleration, and the Gadianton economy. `-RTSV08ReligionTest` and `-RTSV08EconomyTest` are the focused probes.
- Run mirrored Normal matches with `-RTSV09BotBalance -RTSV09BotSupportTrace -RTSBalanceShards=15 -RTSBalanceShard=N -nullrhi -nosound -unattended`. Shards 0, 3, and 4 put Nephites against Lamanites, Shiz, and Laban. Shards 5 and 6 put Lamanites against Gadianton and Coriantumr. Shard 10 puts Gadianton against Shiz. The default horizon is 12,000 simulation steps on Zarahemla Valley.
- Add `-RTSBalanceMap=M` to select a different map by its zero-based registry index. Bountiful is 3, the Narrow Neck of Land is 8, and the Hill Cumorah is 10. The report includes the index and stable map ID. Nonzero maps append `-map-MM` to the report filename so runs of the same pair on different maps can execute concurrently.
- Add `-RTSV09BotLegacyCivilizationTuning` to rerun the previous rules or `-RTSV09BotSecondPassRecruitment` to replay cheaper Nephite recruitment alongside the new HP, Lamanite and Gadianton changes. The older `-RTSV09BotFirstPassRecruitment` flag explicitly selects the default. The report's `civilization_tuning` field identifies the arm. Archive each `Saved/Tests/v09-bot-balance-*.json` between arms.

Compare first contact, Age II timing, local and total army HP, the first Teacher attempt and conversion, peak army population, Seat losses and survivors, and the formal result after defeat grace. Shards 3 and 4 isolate the Nephite change, shard 6 isolates the Lamanite change, and shard 10 isolates the Gadianton change. Shards 0 and 5 show how changes to both opponents interact. Human match balance and map-dependent strategies need human games and broader map coverage.

Current probes can add `-RTSV09BotLegacyOpening` and `-RTSV09BotLegacyRaidPolicy` to isolate the earlier opening and bot policies. They still use the corrected visibility implementation. Use the same executable and explicitly matched policies for new comparisons; these switches do not reproduce every implementation detail of an older binary.
