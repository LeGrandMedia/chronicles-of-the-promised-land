# Gadianton bot raids in v0.9

Recommended and actual model for the diagnosis and bot policy were GPT-6.2 Sol.

## What a raid has to accomplish

A Band of Kishkumen costs 40 Food and 25 Precious Things, reserves one population, and takes 18 game seconds to train at a Storehouse in Age II. Forest Foraging costs 100 Food and 50 Timber and shares the same queue. The original planner funded Forest Foraging first when the settlement was safe, waited for three fielded military units before training a Band, and looked for visible economic buildings in entity order. An attack order to defend the settlement could interrupt the approach, the channel, or the return. [Catalog](../Game/Content/Data/catalog.json) · [Planner](../Game/Source/Chronicles/RTSBot.cpp)

The shared authority accepts a direct raid order on a visible, eligible economic building or a worker carrying cargo. A raid-move acquires a target within the raider's sight and line of sight. Enemy worker cargo is private in snapshots. The balance trace counts loaded workers among visible targets for diagnosis, and the planner has no access to that count when choosing orders. [Command validation](../Game/Source/Chronicles/RTSCommands.cpp) · [Raid movement](../Game/Source/Chronicles/RTSEconomy.cpp) · [Snapshot visibility](../Game/Source/Chronicles/RTSWorldSnapshot.cpp)

The raider must stay in range and keep line of sight for three seconds to start stealing. Damage interrupts progress. Stock taken from buildings, worker cargo and intercepted gathering remain on the Band until it deposits at an eligible drop-off. The standard Band holds 80 resources and automatically returns when full. A raid on a building may draw up to three base resources per second after the channel. Food gathered by regular workers from foreign farms or flocks is a separate source. [Economy and delivery](../Game/Source/Chronicles/RTSEconomy.cpp) · [Damage and dropped cargo](../Game/Source/Chronicles/RTSCombat.cpp)

## Six control replays on Zarahemla Valley

The paired Normal-bot games used the retained first-pass civilization tuning and the original worker formation. The control preserved the original raid policy and added the trace. Each Storehouse finished by 28.3 seconds. Forest Foraging finished at 269.8 seconds in slot 0 and 273.6 in slot 1. The planner then waited for three military units. It queued the first Band at 316.9 or 335.1 seconds and fielded it at 334.8 or 353.0. The Storehouse had enough Food and Precious Things at every first successful queue. [Lamanite reports](Evidence/v0.9/balance/gadianton-raid-control-legacy-opening/map-0/pair-5/) · [Shiz reports](Evidence/v0.9/balance/gadianton-raid-control-legacy-opening/map-0/pair-10/) · [Laban reports](Evidence/v0.9/balance/gadianton-raid-control-legacy-opening/map-0/pair-11/)

| Opponent | Gadianton slot | Bands fielded | Visible when Bands were available | Raid and channel |
|---|---:|---:|---|---|
| Lamanites | 1 | 5 | No worker or raid building | No attempt |
| Lamanites | 0 | 4 | No worker or raid building | No attempt |
| Shiz Jaredites | 0 | 4 | No worker or raid building | No attempt |
| Shiz Jaredites | 1 | 5 | First field at 479.1 s | Two Bands charged from 78–79 m. Three damage resets. Longest channel 1.2 s. |
| House of Laban | 0 | 4 | No worker or raid building | No attempt |
| House of Laban | 1 | 5 | First field at 444.9 s | Two Bands charged from 153–154 m. Seven damage resets. Longest channel 3.0 s rounded. |

All six Gadianton sides lost and delivered zero raid cargo. They also collected zero food from foreign farms or flocks. The two attempted raids ended under fire before the Bands could steal anything, with zero recorded raid interruptions from the base-defense planner. The replay isolated the first problem to finding an exposed target and reaching it safely. The queue produced four or five Bands in every leg, so recruiting Bands earlier would have brought more of them into the same targeting policy. [Control reports](Evidence/v0.9/balance/gadianton-raid-control-legacy-opening/map-0/)

The same executable ran both Shiz legs with the trace disabled. The two reports matched field for field after removing the raid trace and timing fields. These newer controls differ slightly in game outcomes from the earlier first-pass package, so the policy comparison uses one executable for both arms. [Trace-off Shiz control](Evidence/v0.9/balance/gadianton-raid-trace-off-check/map-0/pair-10/)

## Expedition policy trial

The trial assigns one Band to look for nearby economic buildings using ordinary scouting goals. It considers structures already visible to the faction and requires the selected Band to be within its own sight and have line of sight. Visible armed guards influence the approach and retreat. Once the Band has at least 15 cargo, loses the target, or comes under pressure, it can return a partial load through the existing delivery command. After a delivery it considers current targets again. Additional Bands can fight with the regular force. [Expedition policy](../Game/Source/Chronicles/RTSBot.cpp)

The initial comparison used one executable and the resource-relative opening in both arms. The former policy stole 50.9 resources across three matches and delivered zero. The expedition stole 40.3 resources across two matches and returned 21.1 against Shiz. It also exposed a software fault in the local-target test. A building could block the sight ray aimed at its own fog cell and disappear from the planner despite a clear line to the building itself. Both policies lost every match in this initial comparison. [Former policy reports](Evidence/v0.9/balance/gadianton-raid-relative-control/) · [Expedition reports](Evidence/v0.9/balance/gadianton-raid-relative-candidate/)

After the entity-visibility correction, the Windows bot suite passed all 50 assertions. Its five expedition checks cover scouting when no target is visible, identical decisions with a concealed building present, a nearby off-grid Storehouse selected over a distant field, identical guard avoidance with different private disarm timers, and full credit for a delivered load while another Band defends the settlement. The separate visibility suite passed all 17 checks. [Bot checks](Evidence/v0.9/maps/opening-visibility-raids/bots-1600x900/v09-bot-tests.json) · [Visibility checks](Evidence/v0.9/maps/opening-visibility-raids/visibility-1600x900/v09-entity-visibility.json)

Both policies then replayed all six matches with corrected visibility, the resource-relative opening, and Windows executable SHA-256 `CFEC2FF3CB447035C883385733E30314225E0E812199FC587EE5D5361E339B81`. The former planner acquired 226.3 resources in four games. The expedition acquired 24.9 in two games. Both sides of the comparison delivered zero and lost all six. Gadianton's expedition losses came earlier in four games. The dedicated scout policy stays available for comparisons. [Shared-visibility former policy](Evidence/v0.9/balance/gadianton-raid-visible-control/) · [Shared-visibility expedition](Evidence/v0.9/balance/gadianton-raid-visible-candidate/)

## Opportunistic raids

Bands without a raid assignment stay with the regular force. The first two are eligible for a raid when base defense is clear. They favor a nearby visible economic building with line of sight and can choose a farther faction-visible building when they have no nearby option. The planner filters targets near visible guards. A Band brings home cargo once it has 15 resources or when it is interrupted, takes recent damage, or the base needs defense. A Band already delivering keeps its order. [Opportunistic policy](../Game/Source/Chronicles/RTSBot.cpp)

Windows Development executable SHA-256 `2A6395D1A7D253BD67CE9D5D5F5E5D9CDA1C7EEDD9D48CDF23FF3D8A8193E568` replayed both arms with corrected visibility and the resource-relative opening. All 12 reports were valid. The opportunistic policy deposited 106.5 of the 109.9 resources it acquired, across seven trips. Its largest load was 20.0. The former planner acquired 226.3 and deposited zero. The largest observed undelivered loads were 59.6 against Shiz and 53.5 against Laban. Worker raids and foreign farm or flock gathering yielded zero in both arms. [Former policy control](Evidence/v0.9/balance/gadianton-opportunistic-control/) · [Opportunistic results](Evidence/v0.9/balance/gadianton-opportunistic-candidate/)

| Opponent | Gadianton slot | Former cargo acquired / delivered | Opportunistic cargo acquired / delivered | Change in first Seat loss |
|---|---:|---:|---:|---:|
| Lamanites | 1 | 0.0 / 0.0 | 0.0 / 0.0 | 0.0 s |
| Lamanites | 0 | 29.6 / 0.0 | 0.0 / 0.0 | +20.9 s |
| Shiz Jaredites | 0 | 88.4 / 0.0 | 34.7 / 34.7 | −7.2 s |
| Shiz Jaredites | 1 | 3.4 / 0.0 | 3.4 / 0.0 | 0.0 s |
| House of Laban | 0 | 105.0 / 0.0 | 71.8 / 71.8 | −63.8 s |
| House of Laban | 1 | 0.0 / 0.0 | 0.0 / 0.0 | 0.0 s |

Gadianton lost all six games under each policy. Age II and first research timings held in all six pairs, and no Gadianton side reached Age III. In the Shiz leg that earned raid income, peak army population rose from 13 to 14 and Gadianton landed 421 attacks compared with 356. The Laban leg that earned raid income reached the same peak army population of 14 and increased peak workers from 16 to 19. Its campaign started 19.8 seconds earlier and it landed 298 attacks compared with 437. Ending stock fell in both earning legs. These reports confirm the stock received from each raid delivery and leave the contribution of individual purchases or army decisions to the outcome open. [Shiz replay](Evidence/v0.9/balance/gadianton-opportunistic-candidate/map-0/pair-10/) · [Laban replay](Evidence/v0.9/balance/gadianton-opportunistic-candidate/map-0/pair-11/)

The executable used for that matchup comparison passed 49 bot assertions. The failed assertion expected a Band that declined a guarded raid to idle, even though it stood outside the regular army's rally radius and correctly received a rally attack-move. The corrected fixture checks that order, its destination and target ID, and repeats the case with each private disarm timer state. The correction changes the fixture only. [Original bot report](Evidence/v0.9/maps/opening-opportunistic-raids/bots-1600x900/v09-bot-tests.json)

The final Windows executable, SHA-256 `CC8D75CDE64C6AF89877698515480D7853C672A63E667A015C41BDBF051042B1`, passed all 50 bot assertions. The five raid assertions cover staying with the army in the absence of a target, fog-hidden targets, preference for a nearby visible site, guard privacy, and delivery of a six-resource load while another Band defends the settlement. [Final bot report](Evidence/v0.9/maps/opening-raids-final/bots-1600x900/v09-bot-tests.json) · [Executable and exit status](Evidence/v0.9/maps/opening-raids-final/bots-1600x900/run.json)

The opportunistic policy is the default for all three bot difficulties. The remaining competitive gap calls for separate comparisons of the first Band's production gate, troop fielding and where the army takes its first battle. The paired policy comparisons above cover Normal bots on Zarahemla Valley.

## Three additional maps

The final executable played both slot assignments on three maps with the live policy. All six reports were valid, and Gadianton lost all six. Four deliveries brought home 81.0 resources across Bountiful and the Hill Cumorah. Foreign farm and flock gathering yielded zero. This sample used the final policy in every match. [Bountiful](Evidence/v0.9/balance/gadianton-opportunistic-new-maps/map-3/pair-11/) · [The Narrow Neck of Land](Evidence/v0.9/balance/gadianton-opportunistic-new-maps/map-8/pair-5/) · [The Hill Cumorah](Evidence/v0.9/balance/gadianton-opportunistic-new-maps/map-10/pair-10/)

| Map | Winning civilization | Gadianton slot 0 game time / cargo delivered | Gadianton slot 1 game time / cargo delivered |
|---|---|---:|---:|
| Bountiful | House of Laban | 1,083.3 s / 41.9 | 1,088.7 s / 0.0 |
| The Narrow Neck of Land | Lamanites | 900.7 s / 0.0 | 809.4 s / 0.0 |
| The Hill Cumorah | Shiz Jaredites | 775.5 s / 39.1 | 755.6 s / 0.0 |

## First-battle opening trial

In all six final-package games on the additional maps, Gadianton brought 11–13 workers to first contact and the opponent had 19. Gadianton's first observed resource deposit came at 21.5–21.9 seconds, compared with 10.2–11.7 for the opponent. Age II came 45–94 seconds later and the first nonhero military unit came 47–100 seconds later. The planner spends the first available Camp slot on a Wilderness Spy, worth 65 maximum HP and five attack, then trains regular fighters. Total maximum army HP at the first battle ranged from 41% to 94% of the opponent's. The clearest opening deficit was the first Narrow Neck leg, with 315 maximum army HP against 770 and only 16 Food in stock. All six Gadianton sides had at least 75 Precious Things at contact. [Bountiful](Evidence/v0.9/balance/gadianton-opportunistic-new-maps/map-3/pair-11/) · [The Narrow Neck of Land](Evidence/v0.9/balance/gadianton-opportunistic-new-maps/map-8/pair-5/) · [The Hill Cumorah](Evidence/v0.9/balance/gadianton-opportunistic-new-maps/map-10/pair-10/) · [Unit prices and attributes](../Game/Content/Data/catalog.json)

Forest Foraging costs 100 Food and 50 Timber, enough Food to commission two ordinary Camp fighters if their other costs are covered. In the six replays, the planner queued it at 243.1–246.9 seconds and completed it at 268.0–271.8. In slot 0, the planner spent about 18 seconds waiting for Timber before it placed the research order, and its reservation logic paused all production during the wait. The upgrade raises the base Gadianton Food gather rate from 0.540 to 0.648 per active worker-second, excluding multipliers. The base-rate Food payback requires about 926 worker-seconds actively gathering Food, or 154 seconds for six workers continuously on Food. Walking and other jobs add time. Gadianton met the enemy 43–175 seconds after the upgrade completed in these six games. The first Band fielded at 322.2 or 351.2 seconds. [Foraging price and effect](../Game/Content/Data/catalog.json) · [Gathering rates](../Game/Source/Chronicles/RTSEconomy.cpp) · [Production gates](../Game/Source/Chronicles/RTSBot.cpp)

The `combat_first` trial delays Forest Foraging until the planner pays for two ordinary Camp fighters. Spy, Band and hero jobs leave that count unchanged. The normal research priority resumes after the second accepted fighter order. The Foraging price, gather rates, opening worker and age priorities, Band gate, troop choices and raid commands are the same in both arms. `foraging_first` is the default. The added bot fixtures exercise the normal research order, Spy exclusion, the two paid fighter jobs and the restored research with the correct stock debit. [Planner](../Game/Source/Chronicles/RTSBot.cpp) · [Bot fixtures](../Game/Source/Chronicles/RTSBotTests.cpp)

Run the same Windows executable with and without `-RTSV09BotGadiantonCombatFirst` on Zarahemla pairs 5, 10 and 11, then compare the same mirrored opponents on the three additional maps. Keep `-RTSV09BotRaidTrace` on in both arms. The trace records accepted opening jobs and their actual authority debits, first-contact army composition and ready defenders within 24 m of the primary Seat, and resource pulses emitted by workers before first contact. The worker-pulse measure covers the tracked worker events across the fixed simulation steps. Ending stock also reflects spending, losses and other income. Compare first Bowman and Cimeter jobs, workers and near-Seat army strength before first contact, economic and military losses, delivered raid income, and Seat survival. Earlier orders alone would leave the competitive question open.

### Zarahemla result

Windows Development executable SHA-256 `5DD0C8FC8851CE4F6C1F92E792E250F332B36EA61504D7038174DA1DA80654AC` completed all 12 valid games. Gadianton lost all six games in each arm. Its Seat fell earlier in five of the six paired games, by a median of 26.6 seconds. The live bot keeps Foraging first. The trial remains available for replay. [Foraging-first reports](Evidence/v0.9/balance/gadianton-combat-first-control/map-0/) · [Combat-first reports](Evidence/v0.9/balance/gadianton-combat-first-candidate/map-0/)

| Opponent | Gadianton slot | Workers at first contact | Maximum army HP at first contact | First Seat loss change |
|---|---:|---:|---:|---:|
| Lamanites | 1 | 12 → 13 | 575 → 545 | +5.6 s |
| Lamanites | 0 | 12 → 14 | 605 → 530 | −104.0 s |
| Shiz Jaredites | 0 | 12 → 14 | 605 → 385 | −19.7 s |
| Shiz Jaredites | 1 | 12 → 13 | 730 → 690 | −5.4 s |
| House of Laban | 0 | 12 → 14 | 605 → 530 | −33.6 s |
| House of Laban | 1 | 12 → 13 | 730 → 690 | −53.8 s |

The trial shifted early Food into worker recruitment. The Seat ordered its first Age II worker 18.0 seconds earlier in slot 0 and 9.0 seconds earlier in slot 1. The first regular fighter order stayed at 282.7 seconds in slot 0 and 308.1 in slot 1. Age II, Camp completion and first Spy order also held. Each slot ordered a different first fighter, and the observed second Band orders came later. Forest Foraging completed 109.8–127.8 seconds later. The trial lowered Gadianton's maximum HP both across the army and among ready defenders near its Seat in all six legs. Five first contacts occurred within 1.8 seconds of their counterpart. The Shiz slot 0 contact came 49.5 seconds earlier and represented the least comparable roster timing. The share of opposing army HP fell in all six legs. [Paid jobs and first-contact rosters](Evidence/v0.9/balance/gadianton-combat-first-candidate/map-0/)

Before first contact, Food pulses from Gadianton workers were 20.0–170.0 lower and Precious Things pulses were 30.0–70.0 higher. Food in stock was 16.6–90.0 lower despite the larger workforce. Raid deliveries earned 57.6 resources in one match, compared with 106.5 across two Foraging-first matches. The results support testing resource allocation as a separate opening decision. The candidate did not advance to the three-map comparison. [Resource pulses and deliveries](Evidence/v0.9/balance/gadianton-combat-first-candidate/map-0/)

## Local Food assignment trial

In the Foraging-first controls, Gadianton workers emitted about 828–958 Food and 260–292 Precious Things before first contact. The opponents' workers emitted about 1,531–2,140 Food. Gadianton had 155–167 Precious Things in stock at first contact, enough for multiple early military purchases, and 27.6–100.0 Food. The standard start already supplies the 200 Precious Things needed for Age II. The generic idle-worker scorer still seeks a 100-resource buffer above the age price, and it awards a bonus to the completed Hill of Ore. [Foraging-first opening reports](Evidence/v0.9/balance/gadianton-combat-first-control/map-0/) · [Assignment scorer](../Game/Source/Chronicles/RTSBot.cpp)

The `local_food_assignment` trial applies an additional check to an idle worker's ordinary ore assignment in Ages I and II. It checks for a Food shortfall and enough Precious Things in stock to pay for Age II, or for one Spy, two regular frontline recruits and one Band after advancing. If that worker can gather visible, neutral Food within 24 m of the primary Seat, the trial chooses the best legal local Food or Timber assignment using the ordinary scores. The worker follows its usual assignment when Food is funded, Precious Things are below the reserve, the base is under threat, or the worker lacks an eligible local Food site. Existing assignments and loads continue through the shared authority. The live default, gathering rates, eligible farms, recruitment, and research priorities stay the same. [Assignment](../Game/Source/Chronicles/RTSBot.cpp) · [Authority gathering](../Game/Source/Chronicles/RTSEconomy.cpp) · [Bot fixtures](../Game/Source/Chronicles/RTSBotTests.cpp)

For the paired replay, use `-RTSV09BotGadiantonFoodAssignment` in the trial arm, with the same executable and `-RTSV09BotRaidTrace` in both arms. Run it independently of Combat First. The report includes accepted switches away from surplus ore, commitments to Food, Timber and Precious Things at first contact, worker resource pulses, and the paid opening jobs. Compare Age II timing, first regular fighters, near-Seat strength at first contact, and later outcomes. The trial can shift the bottleneck into Timber or Precious Things. The paired receipts and orders can identify that result.

## Replay commands

`-RTSV09BotBalance -RTSV09BotRaidTrace -RTSBalanceShards=15 -RTSBalanceShard=N -nullrhi -nosound -unattended` records both slot assignments for shard `N`. Pairs 5, 10 and 11 face the Lamanites, Shiz and the House of Laban. `-RTSV09BotLegacyRaidPolicy` selects the former planner, and `-RTSV09BotRaidExpedition` selects the dedicated scout trial. `-RTSV09BotLegacyOpening` also restores the older starting formation and nearby resource layout. The report records `gadianton_raid_policy`, `opening_formation`, and the per-side `gadianton_raids` trace.


## Final handoff cutoff

The additional Food Assignment diagnostic completed six matched games per arm before the cutoff. All reports were valid; Gadianton lost all six in each arm. Relative game-duration changes were +253.64, -145.56, -377.41, -62.58, +16.80 and +14.70 seconds across the ordered pair/leg reports. This mixed result does not justify changing the default opening. Both experimental opening flags remain opt-in; ordinary matches retain Foraging-first and opportunistic raids. See [paired outcomes](Evidence/v0.9/balance/gadianton-food-assignment-summary.json), [control reports](Evidence/v0.9/balance/gadianton-food-assignment-control/), and [candidate reports](Evidence/v0.9/balance/gadianton-food-assignment-candidate/). These games used their recorded pre-handoff executable and are not new match evidence for the final binary.
