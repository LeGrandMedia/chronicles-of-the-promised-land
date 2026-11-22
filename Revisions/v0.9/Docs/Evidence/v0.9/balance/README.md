# v0.9 bot round robin

The probe plays both starting-slot assignments for each of the 15 pairs among the six civilizations. Both bots play on Normal difficulty with fog active, regular fruitfulness, 300 Food, 300 Timber, 200 Precious Things, and zero Cement. Each leg starts from the stock Zarahemla Valley layout. The factions exchange seats and the map stays fixed. The layout and planner use deterministic rules with no seed option.

Only the normal match-start fixture is populated. After that, both sides take actions through the host bot planner and shared command validation. The observer samples the authoritative simulation after each fixed step and does not expose any information to the planners. The competitive victory rule decides wins. A match still in progress after 12,000 steps is recorded as a time limit with both sides' final state. A time limit is not scored as a faction win.

The observer records attack timestamps, ownership conversions, resources, worker and army counts, army population, nominal catalog cost, and losses. Combat losses are removals marked by damage in the same step or a previously scheduled mortal wound. Damage estimates net health lost in marked steps after normalizing health caps for Favor, auras, and construction. Healing in the same step can reduce the estimate. Brief entities born and destroyed within a step can escape observation. Remaining owned units cleared by defeat are tracked separately from combat losses. Nominal army cost is a catalog comparison and includes the listed prices of surviving heroes and converted troops.

Use the Development executable with `-RTSV09BotBalance -nullrhi -nosound -unattended`. This writes a JSON checkpoint after each match in `Saved/Tests/v09-bot-balance-00-of-01.json`. Run independent processes with `-RTSBalanceShards=5` and `-RTSBalanceShard=0` through `4` for five disjoint sets of matchups. Keep the step limit at its 12,000-step default for an equivalent run. A single process using `-RTSBalanceShards=15` and one shard index runs both legs of one pair in isolation.

Step timing wraps the simulation call. It includes the planner and simulation rules and excludes observer bookkeeping and graphical rendering. Timings from parallel processes reflect that shared CPU load. The results evaluate the Normal bot strategies on one map and a 20-game-minute horizon. Human balance, naval combat, other maps, higher difficulties, and late games call for separate evidence.

## Baseline results

The packaged Windows executable completed all 30 matches. Thirteen matches ended in victory and 17 reached the time limit. Each of the 60 bot sides advanced to Age II, researched an upgrade, and landed an attack. Seven timeouts ended with at least 180 game seconds since the last damage to either side. Five pairs had the same winner in both slot assignments. Coriantumr versus the House of Laban split one win apiece. Slot 0 won eight matches and slot 1 won five.

| Faction | Wins | Losses | Time limits | Median Age II (s) | Median first hit (s) | Median peak army units | Median peak army population |
|---|---:|---:|---:|---:|---:|---:|---:|
| Nephites | 0 | 6 | 4 | 181.0 | 357.0 | 10 | 12.5 |
| Lamanites | 7 | 0 | 3 | 151.5 | 315.0 | 41 | 44 |
| Gadianton Robbers | 0 | 0 | 10 | 262.1 | 339.5 | 8 | 11 |
| Coriantumr Jaredites | 1 | 3 | 6 | 166.7 | 334.7 | 11.5 | 24.5 |
| Shiz Jaredites | 2 | 1 | 7 | 166.7 | 331.1 | 10 | 21 |
| House of Laban | 3 | 3 | 4 | 169.6 | 331.1 | 14 | 18.5 |

The Jaredite unit and population columns diverge because their troops have higher population costs. Gadianton completed research in every game and took raid cargo in three. Its largest observed raider load was 31.4 total resources. The armed forces of every other faction landed attacks against Gadianton, yet the Gadianton Seat survived all 10 legs. Against Lamanites, both Jaredite factions, and Laban, Gadianton reached the deadline with one Seat and zero workers or fighting troops in all eight legs. In the two Nephite matchups, Gadianton still had 19 workers and damage continued into the final 10 game seconds. These results identify two situations with living Seats and very different end states.

The unopposed Seat cases expose a gap in the baseline bot's orders. The scout policy explored broad areas and sent no dedicated search around the last observed Gadianton settlement. Routine recruitment could also defer a replacement Spy when the army reached its target size or saved for an age advance. The shared engine gives the Wilderness Spy a mobile six-meter detection radius. The eight-meter detectors are stationary Towers of Watchmen and Cement Towers. Religious units provide conversion and other abilities through their own rules. Each faction can recruit the Spy from an Encampment of War.

The detector change keeps a clue from a previously visible Gadianton building, prioritizes an observed Seat's position, and can queue one ordinary replacement Spy during a campaign. Each search allows up to 25 waypoints and 90 game seconds, with 45 seconds between searches. A Spy holds detection only when a currently visible Seat lies within the standard range and line of sight. The scouts and armies retain the normal visibility, movement, price, production, and population rules.

The baseline used five concurrent processes and collected 306,406 simulation steps. Per-match step medians ranged from 1.651 to 4.508 ms. The 95th percentiles ranged from 21.182 to 29.832 ms. The highest single sample was 168.467 ms under shared CPU load. These measurements describe the headless tournament on this host. [Run configuration and hashes](run-metadata.json), [complete baseline summary](summary.json)

### Nephite results

The Nephite bot lost both legs against Lamanites, Shiz, and Laban. It peaked at 8–14 army population in those losses, compared with 22–44 for the opposing bots. It reached 19 workers in five of the six losses and 16 in the remaining one. The main measurable gap was in fielded military population. It also reached Age II at about 172 seconds in slot 0 and 190 seconds in slot 1 even as the opponent changed.

The catalog makes most Nephite units cost 25% more and train 20% slower than the base rates. The Lamanite rates are 35% cheaper and 40% faster, with Anger accelerating training further. The bot waits for an army before recruiting religious support and generally leaves its support units near the settlement. This can leave conversion-based advantages underused. Ending Nephite Favor in the losses ranged from 23.3 to 49.0 and cannot establish Favor or army health at first contact. The experiment measures the combined economy, unit mix, doctrine, and canonical traits. A test that changes one planner decision at a time would be needed to assign causal weight or justify stat changes.

Generic attack-move excludes workers from automatic target selection. Opponents of Nephites recorded zero worker combat losses across their ten games. Automatic combat can acquire buildings. The two Gadianton opponents lost seven and eleven economic buildings, each eligible for the canonical eight-point Favor deduction. The six decisive Nephite losses had zero opposing worker or economic-building combat losses. The recorded civilian and economic kills therefore cannot explain those defeats. The reports do not isolate the net Favor change from conversion, food gathering, decay, or loss of a Nephite Temple.

## Detector replay

The updated packaged executable replayed both assignments against every Gadianton opponent using the same catalog hash and setup. Eight games ended in victory. Two reached the time limit while damage remained active. The count of timeouts with an isolated Gadianton Seat, zero workers, and zero troops fell from eight to zero. [Replay provenance](detector-retest-metadata.json), [before-and-after results](detector-retest-summary.json)

| Opponent against Gadianton | Leg 1 | Leg 2 |
|---|---|---|
| Nephites | Nephite win at 769.3 s | Time limit |
| Lamanites | Lamanite win at 495.3 s | Lamanite win at 785.4 s |
| Coriantumr Jaredites | Time limit | Coriantumr win at 740.5 s |
| Shiz Jaredites | Shiz win at 821.7 s | Shiz win at 787.3 s |
| House of Laban | Laban win at 881.3 s | Laban win at 731.8 s |

In the remaining Nephite timeout, the Gadianton Seat fell at 1,184.0 seconds, inside the 60-second defeat grace period. In the remaining Coriantumr timeout, Gadianton retained four workers and one Seat and took damage in the final step. These results verify the closing tactic for the tested isolated-Seat failure. They do not measure the strength of a human Gadianton player.

Replay step medians ranged from 1.254 to 2.564 ms and 95th percentiles from 20.175 to 21.530 ms, with a maximum of 160.217 ms. This batch also used five processes. The outcomes and lengths changed substantially, so elapsed timings cannot isolate the cost of the detector logic.

## First-contact faith diagnosis

The six legs against Lamanites, Shiz, and Laban that Nephites lost in the baseline were replayed through 480 game seconds with extra observations and no faith-policy changes. The observer records both rosters and resources immediately before the first step with a successful attack or recorded damage. The table counts all live military units on a side. The first hit may involve only a scout or a few of those units. Native compilation overlapped the runs, so their wall-clock timings carry additional contention. Simulation times and game state remain comparable. [Run metadata](faith-contact-metadata.json)

| Opponent | Leg | Contact (s) | Nephite Favor | Nephite and opposing army population | Nephite and opposing army max HP | Nominal army cost |
|---|---:|---:|---:|---:|---:|---:|
| Lamanites | 1 | 320.5 | 24.47 | 7 / 11 | 129 / 770 | 727 / 538 |
| Lamanites | 2 | 309.5 | 22.18 | 6 / 11 | 92 / 770 | 620 / 538 |
| Shiz Jaredites | 1 | 418.0 | 25.22 | 11 / 17 | 233 / 986.5 | 1,141 / 970 |
| Shiz Jaredites | 2 | 331.1 | 22.60 | 7 / 11 | 118 / 689.5 | 720 / 690 |
| House of Laban | 1 | 346.8 | 24.61 | 8 / 9 | 149 / 685 | 834 / 745 |
| House of Laban | 2 | 331.1 | 22.60 | 7 / 9 | 118 / 685 | 720 / 745 |

Nephites fielded fewer troops by population and much less total max HP in all six legs, even though their nominal army cost was higher in five. At first contact they had zero Teachers in all six legs and one finished Temple in total. That Temple had finished about 0.4 seconds earlier. The five sides without a finished Temple held 32–96 Timber at contact, against the Temple's 140 Timber cost. Foundation and construction-order state were outside the capture.

In the three legs that finished the Temple by 480 seconds, the first Teacher followed about 42–44 seconds later, consistent with the faction's 42-second training time. A conversion channel began in only one of those legs before the probe ended. The observed delay is concentrated in getting the first Temple online. A controlled test of first-Temple construction priority would separate the cost of earlier passive Favor and conversion access from the cost of deferring a Workshop. Escort decisions can be tested separately once a Teacher is present. The existing unit stats and normal bot policy were retained for this measurement. [Lamanite contact logs](faith-contact-pair-00.json), [Shiz contact logs](faith-contact-pair-03.json), [Laban contact logs](faith-contact-pair-04.json)

## Controlled first-Temple priority

The development trial gives the Nephite bot up to 120 game seconds to fund its first Temple ahead of the Workshop, after the first Camp is paid. Housing, existing opening worksites, and Seat repairs retain their earlier precedence. A visible threat near the base cancels the reservation so the bot can resume defensive construction. This is one change to construction priority. Unit stats, religious generation, resources, training orders, support movement, and civilian targeting follow the existing rules.

The trial uses the same six slot assignments as the first-contact tests and runs each leg to victory or the 12,000-step cap. A short control using the same executable with the trial flag absent checks that the original first-contact states reproduce and records the unchanged Camp, Temple, and Workshop timings. The original 30-match baseline supplies full-horizon outcomes for the control comparison.

All six matched controls reproduced the original first-contact measurements. The trial matched all six first-Camp starts and completions. Peak workers stayed equal in five legs and increased by one in the remaining leg. Temples ready at contact rose from one to four. Median first-contact Favor increased by 3.60 and army max HP by 34, with lower first-contact HP in two legs. No Nephite Teacher reached first contact in either group. First-Workshop completion slipped by 54.0–156.0 game seconds.

Five trial legs remained losses and one Laban loss became a timeout. In that timeout, the last Nephite Seat fell at 1,158.6 seconds. Two workers and zero troops remained at the deadline, within the 60-second defeat grace period. The results did not justify changing the default priority. The experimental option was removed from live source, retaining the measurement fields and the archived trial. The default bot regression run also passed all 31 checks on the trial package with the flag off. [Trial configuration](temple-trial-metadata.json), [matched trial summary](temple-trial-summary.json), [default regression report](temple-default-bot-regression.json)
