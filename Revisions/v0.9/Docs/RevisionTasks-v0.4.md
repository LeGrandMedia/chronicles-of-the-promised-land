# v0.4 playtest checklist

This ledger tracks all 44 implemented entries from `PlaytestNotes-v0.4.txt`. Each task retains its recommended model. Evidence distinguishes automated checks from source or visual review; see [Validation v0.4](Validation-v0.4.md) for totals, limitations, and saved reports.

| Note | Requirement | Best model / effort | Status | Area owner |
|---:|---|---|---|---|
| 1 | Gadianton workers cannot farm or tend flocks; theft supplies their economy. | gpt-6.1-sol / high | Implemented; gather/sheep restrictions and Age-I raider recruitment pass native checks | Catalog/runtime economy |
| 2 | Display Laban laborers as Henpecked Gentiles. | gpt-6-luna / medium | Implemented; faction display-name checks pass | Catalog/names |
| 3 | Laban Vineyards employ workers; Wineries passively generate Food. | gpt-6-luna / medium | Implemented; Vineyard naming checked; Winery generation reviewed | Catalog/economy |
| 4 | Slightly reduce invisible-unit opacity. | gpt-6-luna / medium | Implemented; viewport opacity and multiplayer stealth checks pass | Visual manifest |
| 5 | Rename the Gadianton military unit Band of Kishkumen. | gpt-6-luna / medium | Implemented; catalog and faction naming checks pass | Catalog/names |
| 6 | Route raiders between enemy and friendly Storehouses for repeated theft. | gpt-6.1-sol / high | Implemented; repeated raid route returns cargo and revisits its source in simulation | Commands/economy |
| 7 | Lamanite Temple displays as Rameumptom and can be upgraded defensively. | gpt-6.1-sol / high | Implemented; temple upgrade converts targets into Zoramite Captains in native checks | Catalog/conversion |
| 8 | Laban Great and Spacious Building supports Drunken Concubines and area effects. | gpt-6.1-sol / high | Implemented; aura health fraction and Seduce conversion checks pass; movement/attack reductions reviewed | Catalog/combat |
| 9 | Allow construction in explored fog without line of sight. | gpt-6.1-sol / high | Implemented; explored but unseen construction passes native checks | Construction/fog |
| 10 | Notify robbery victims. | gpt-6.1-sol / high | Implemented; victim notification and 8-second throttle reviewed | Network/UI |
| 11 | Faster, weaker, knife-armed Band of Kishkumen trains from Storehouses. | gpt-6-luna / medium | Implemented; Age-I Storehouse training checked; weaker/faster stats and knife rendering reviewed | Catalog/combat |
| 12 | Add raid move: raid the first eligible enemy economic structure seen, then return to nearest drop-off. | gpt-6.1-sol / high | Implemented; raid-move target acquisition checked; own-sight and nearest-drop-off rules reviewed | Commands/AI |
| 13 | Show stolen cargo when selecting the raider. | gpt-6.1-sol / high | Implemented; selected-raider Food/Timber/Precious Things cargo display reviewed | HUD |
| 14 | Add 2× and 3× raid theft upgrades. | gpt-6-luna / medium | Implemented; threshold, exact victim debits, bonus cargo, and capacity checks pass | Catalog/economy |
| 15 | First age-up choices should be faction progenitors. | gpt-6-luna / medium | Implemented; progenitor choices catalog-audited for all six factions | Catalog/progression |
| 16 | Fix construction progress becoming stuck. | gpt-6.1-sol / high | Implemented; construction completion, assistance, and beast aura checks pass | Construction/simulation |
| 17 | Add three hero-choice age advancements. | gpt-6.1-sol / high | Implemented; all three advancements and duplicate locks pass for every faction | Catalog/progression |
| 18 | Let Traders set routes and earn Precious Things near workers. | gpt-6.1-sol / high | Implemented; route editing passes viewport checks; reward/cooldown checks pass | Commands/economy |
| 19 | Click resource worker counts to cycle assigned workers. | gpt-6.1-sol / high | Implemented; real viewport workforce click selects an assigned worker | HUD/selection |
| 20 | Give each faction distinctive building/unit display labels and descriptions. | gpt-6-luna / medium | Implemented; owner-aware names/descriptions and faction catalog checks pass | Catalog/names/HUD |
| 21 | Use horse art for King’s Chariot and support boarding/ungarrisoning. | gpt-6.1-sol / high | Implemented; horse placeholder visually reviewed; right-click boarding and E unloading pass | Visuals/commands |
| 22 | Hide heroes already chosen in new Seats of the People. | gpt-6.1-sol / high | Implemented; global age lock checked on multiple headquarters and hidden cards checked in viewport | Production/progression |
| 23 | Increase civic-resource decay above the starting level. | gpt-6-luna / medium | Implemented; high-stat decay check passes | Catalog/simulation |
| 24 | Replace Jaredite beast evolution with separate age-tier units and Cement. | gpt-6.1-sol / high | Implemented; separate recruitment, passive resources, Cement, construction, and carrier checks pass | Catalog/resources |
| 25 | Cap flock buildings at five, or ten for Jaredites. | gpt-6.1-sol / high | Implemented; faction-dependent server limits include foundations; five-flock rejection tested | Catalog/build validation |
| 26 | Add more trees to maps. | gpt-6.1-sol / high | Implemented; additional mirrored tree groves reviewed and map starts tested | Maps |
| 27 | Build walls between two endpoints and queue endpoints with Shift. | gpt-6.1-sol / high | Implemented; first/second endpoints and Shift continuation pass real viewport checks | Construction/UI |
| 28 | Prevent laborers getting stuck at Flocks. | gpt-6.1-sol / high | Implemented; ten workers enclosed in a completed Flock all escape, gather, and deliver | Navigation |
| 29 | Scatter enemy flocks to steal their sheep beyond the standard cap. | gpt-6.1-sol / high | Implemented; captured sheep transfer owner and keep producing independently of flock slots | Economy/death handling |
| 30 | Show Lamanite Priestcrafter upgrades in the Lamanite Temple. | gpt-6-luna / medium | Implemented; faction upgrade producers and research filtering reviewed | Catalog/research UI |
| 31 | Remove the Judgment Seat. | gpt-6-luna / medium | Implemented; Judgment Seat absent from active catalog and construction controls | Catalog |
| 32 | Make Encampment of War an Age-II building. | gpt-6-luna / medium | Implemented; Age-I Encampment construction is rejected without payment | Catalog/build validation |
| 33 | Show the faction's civic statistic name in the top UI. | gpt-6.1-sol / high | Implemented; faction-specific devotion labels checked | Names/HUD |
| 34 | Rename Laban's Seat of the People Laban's Crib. | gpt-6-luna / medium | Implemented; Laban's Crib display override checked | Names/catalog |
| 35 | Rename Drunken Concubine's conversion ability Seduce and convert to a Henpecked Gentile. | gpt-6.1-sol / high | Implemented; Seduce label reviewed; conversion creates a regular laborer in native checks | Names/abilities |
| 36 | Filter faction-specific descriptions so unrelated content is not shown. | gpt-6.1-sol / high | Implemented; faction rosters/descriptions, owner-aware inspection, help, hotkeys, and action filters audited | Catalog/HUD |
| 37 | Show units garrisoned inside a Chariot in the selected-unit HUD. | gpt-6.1-sol / high | Implemented; actual passenger panel passes viewport and multiplayer privacy checks | HUD/transport |
| 38 | Add ranged/religious/melee counter relationships. | gpt-6.1-sol / high | Implemented; ranged/religious damage, ranged resistance, and faster melee conversion checked | Combat/balance |
| 39 | Shift-click queues five units at a time. | gpt-6.1-sol / high | Implemented; real Shift-click adds five paid queue entries | Production/input |
| 40 | Consider a beast construction aura to address stalled Jaredite building. | gpt-6.1-sol / high | Implemented; idle nearby beasts advance eligible construction sites automatically | Simulation/design |
| 41 | Refund canceled units/technologies fully and buildings proportionally. | gpt-6.1-sol / high | Implemented; active jobs refund full payment; unfinished structures refund remaining fraction | Economy/production |
| 42 | Gate Gadianton vision and drop-off infiltration by Lasciviousness. | gpt-6.1-sol / high | Implemented; threshold activation/revocation, stealth detection, 50/100% diversion, and shared network vision checked | Catalog/stealth |
| 43 | Remove Judgment Seat; train Lawyers at the Gadianton Lodge. | gpt-6.1-sol / high | Implemented; Lodge trains Lawyers; discovery stops Lodge generation; producer checks pass | Catalog/production |
| 44 | Rename Gadianton workers; preserve conversion disguise; add Signs and Symbols deposit routing. | gpt-6.1-sol / high | Implemented; names checked; server ownership, former-owner delete exception, disguised network views, and Signs routing checked | Names/economy/visibility |

## Model routing

Catalog, names, balance prose, and source attribution: **gpt-6-luna / medium**. New simulation, command, network, or HUD behavior: **gpt-6.1-sol / high**. Content work was delegated to GPT-6 Luna; gameplay and interface work to GPT-6.1 Sol. Integration, build, and regression verification were handled by the coordinating agent. The rows above record the completed implementation evidence.




