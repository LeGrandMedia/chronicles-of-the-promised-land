# Computer opponents in v0.9

Computer opponents fill the same four lobby seats as human players. The host can add, remove, or configure a computer's faction, team, and difficulty before the game starts. A computer can join either team in a solo or LAN game. Computers are always ready. A lone human without a computer retains the sandbox.

| Difficulty | Decision interval in game seconds | Target workforce | Target army | Minimum opening wave | Earliest offensive launch in game seconds |
|---|---:|---:|---:|---:|---:|
| Easy | 3.5 | 14 | 22 | 12 | 330 |
| Normal | 1.8 | 19 | 40 | 8 | 205 |
| Hard | 0.9 | 25 | 65 | 6 | 155 |

Waves count available combat units. Walk time, enemy pressure, population, buildings, and gathered resources determine the actual attack schedule. Early defense can begin as soon as the computer sees a threat and has a unit to send. Difficulty changes decision cadence, growth, and deployment thresholds. All three levels use the normal faction costs, production times, population limits, and gathering rates.

## Match behavior

- Laborers choose visible work and rebalance as their assignments end. They scout when accessible supplies are scarce. The planner builds housing, worksites, drop-off points, research, and military production. It replaces lost builders, assigns an available builder to abandoned foundations, repairs a threatened Seat, and can start a replacement Seat.
- The six factions use their legal hero choices, units, and research. Jaredite construction uses the required builder beasts, with later flocks and Cement defenses. Gadianton workers use wild provisions and permitted foreign sites, with dedicated Robbers for visible raid targets. Nephite armies prioritize military targets and the enemy Seat to limit Favor losses. The House of Laban builds a Winery and pursues its faction upgrades.
- The first Wilderness Spy scouts independently with the normal detection radius. Armies rally, defend against visible threats, and move towards explored targets or new positions. A computer can revisit a building's last observed position. Direct target orders require current visibility.
- Religious units follow the main force from behind and can make short approaches to visible, eligible conversion targets when the path and line of sight are clear. The planner uses nearby troops as cover and shelters unsupported converters from visible attackers. Its support moves end in a queued hold so the next decision can check threats at the new position. Existing queues and active conversion channels keep priority.
- One Lamanite Priestcrafter can take the completed Rameumptom to provide its stronger military aura. House of Laban Concubines can cover clusters of working laborers with their economy aura. Ammonites remain in the worker planner and retain valid gathering assignments.
- The host runs the planner on simulation ticks. A computer has no network client or controller. Lobby controls use authenticated host orders. Both computer and human match orders pass through the same ownership, cost, age, construction, and target checks. Fog, stealth, and disguise follow the human snapshot rules.
- The current planner uses land routes, including the river map's land crossings. It does not plan naval invasions, transport boarding, or bespoke tactics for every hero ability.

## Repeatable verification

Run the v0.9 Development executable with `-RTSV09BotTest -nullrhi -nosound -unattended`. The process exits nonzero on an assertion failure and writes `Saved/Tests/v09-bot-tests.json`. The native probe checks lobby authority and slot reuse, mixed human/computer readiness and ownership, difficulty, fog memory, defense, and unmodified resource and population limits. Its support scenarios test conversion through the authoritative simulation, channel preservation and target claims, concealment and line-of-sight blocks, ranged threats and escorts, holding after movement, both faction auras, and Ammonite work. It runs a standard opening for each faction on the three map presets and records the first earned deposit, completed new building, trained worker, age advance, trained military unit, completed research, and successful attack. It runs the Gadianton river match until victory or the test deadline.

The opponent in these opening probes stays at its starting settlement, making timings repeatable. These probes establish that each faction can execute a complete opening and land an attack. Human-versus-computer balance requires playtesting.

The packaged Windows run passed all 45 checks, including 14 support-unit assertions. All six factions earned deposits, completed buildings, recruited laborers and military units, advanced to Age II, finished research, and landed attacks. The Gadianton bot advanced to Age III and won the Land of Many Waters match at 939.9 game seconds. The other runs ended after their opening milestones. [Current probe report](Evidence/v0.9/bots/support-arrival-hold/v09-bot-tests.json) · [Runtime log](Evidence/v0.9/bots/support-arrival-hold/Bots-v09-support-arrival-hold.log) · [43-check support run](Evidence/v0.9/bots/support-first/v09-bot-tests.json) · [31-check baseline](Evidence/v0.9/bots/v09-bot-tests.json)

| Faction | Map | Age II (game seconds) | First completed research | First successful attack |
|---|---|---:|---:|---:|
| Nephites | Zarahemla Valley | 172.6 | 198.1 | 392.6 |
| Lamanites | Waters of Sidon | 147.6 | 239.6 | 468.1 |
| Gadianton Robbers | Land of Many Waters | 248.6 | 280.6 | 439.1 |
| Coriantumr Jaredites | Zarahemla Valley | 166.6 | 192.6 | 379.6 |
| Shiz Jaredites | Waters of Sidon | 166.6 | 192.6 | 590.6 |
| House of Laban | Land of Many Waters | 159.1 | 185.6 | 509.1 |

The normal Gadianton opening reached Age II 76 to 101 game seconds after the other factions and still landed a hit before three of them. The maps, unit mixes, and time spent discovering the opposing base differ across these runs. First-hit times describe opening pressure in the tested scenario. They do not rank the factions for competitive play.

The six scenarios sampled 32,800 fixed simulation steps on the development Windows machine. Faction medians ranged from 0.40 to 0.87 ms, 95th percentiles from 15.34 to 19.55 ms, and maxima from 21.08 to 64.41 ms. These values describe the headless opening scenarios, including the planner, economy, combat, and navigation, with other tests active on the same machine. A four-player late-game battle and rendering frame time require separate measurements.

## Competitive support-unit replay

Eight same-executable matches compared the original support planner active and bypassed. Lamanites and House of Laban each defeated the Nephites in both starting-slot assignments in both modes. Eight follow-up matches changed each faction's support orders independently. With Laban support bypassed, Nephite support orders enabled six conversions in the slot-0 match and delayed Seat loss by 140.8 game seconds. With Laban support enabled, Laban won sooner in both starts and the Nephites completed zero conversions. The traces also identified an idle conversion between bot decisions after a Teacher arrived at its rally. After the hold fix, the Teacher waited for its next decision. The two rerun matches retained their earlier winners, finish times, and conversion counts. [Controlled comparison](Evidence/v0.9/balance/support-policy/controlled/README.md) · [Per-faction trace](Evidence/v0.9/balance/support-policy/role-trace/README.md) · [Arrival fix replay](Evidence/v0.9/balance/support-policy/arrival-hold/README.md)

## LAN regression

Run `Tools/Test-LAN-v09.ps1` from the v0.9 revision after packaging. Separate Windows processes open a listen server and a loopback client. They exercise the normal lobby RPCs, the packed snapshot format, an allied Hard Coriantumr bot, unauthorized guest changes, a human movement order, and the normal fog and resource filters. Run the same script with `-Mode humans` for the four-human connection and command regression.

The mixed test passed on both processes. The guest received 247 snapshots, observed the bot gather, build, and train, and moved their own worker while the allied bot worker stayed under server control. The host confirmed the bot's private resources and unseen units absent from the opposing view. [Guest results](Evidence/v0.9/network/mixed/network-v09-bots-guest.json), [host results](Evidence/v0.9/network/mixed/network-v09-bots-host.json)

An earlier four-human automated run signaled Ready during client startup. The server began immediately while the clients spent about 16 seconds importing local presentation assets. All three clients moved, trained, queued waypoints, and received victory. The early finish and overlapping failure and apology requests caused the construction and Liahona observations to fail. The probe now waits for each local controller to finish startup before it signals Ready and waits for the server's Liahona failure state before sending the apology. [Earlier reports and logs](Evidence/v0.9/network/legacy-before-ready-fix)

The four-human retest passed on the host and all three clients. Each process observed movement, queued waypoints, production, completed construction, rally replication, Liahona failure and recovery, and team 0 victory. [Four-human LAN reports](Evidence/v0.9/network/humans)
