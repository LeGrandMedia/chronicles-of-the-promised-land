# Emergency Slinger balance trial

**Keep the priority opt-in.** It delayed the last Nephite Seat loss in all four mirrored starts. Lamanites and House of Laban still won every trial game, and the first Teacher's queue, arrival, and channel times stayed the same wherever a Teacher existed. The original second-slot Laban case still missed its Teacher after the bot queued 16 emergency Slingers.

The same Windows Development executable, SHA-256 `012186FA30DC3C72B6C8B90B7A8A0352495B4922AD0EE68BE8ACD76252BBD85E`, ran eight valid games with Normal bots, standard resources, fog, and the stock Zarahemla Valley layout. Both factions used their current support orders. All four processes exited 0, and both trial processes passed the two authority queue checks for Slinger priority and Sword fallback. The trial queued 27 Slingers in 104 attempts. Each start accepted a Slinger at its first eligible trial decision. Recommended and actual model was GPT-6.2 Sol. [Comparison JSON](comparison.json) · [Lamanites control](baseline/pair-0/v09-bot-balance-00-of-15.json) · [Lamanites trial](trial/pair-0/v09-bot-balance-00-of-15.json) · [Laban control](baseline/pair-4/v09-bot-balance-04-of-15.json) · [Laban trial](trial/pair-4/v09-bot-balance-04-of-15.json)

| Opponent | Nephite slot | Eligible Camp decisions, baseline → trial | Trial Slingers queued / attempts | Last Seat loss, baseline → trial (s) | Match finish, baseline → trial (s) | Nephite worker combat losses, baseline → trial |
|---|---:|---:|---:|---:|---:|---:|
| Lamanites | 0 | 33 → 21 | 2 / 21 | 532.12 → 535.32 | 592.10 → 595.30 | 23 → 24 |
| Lamanites | 1 | 3 → 5 | 2 / 5 | 460.92 → 472.92 | 520.92 → 532.92 | 20 → 17 |
| House of Laban | 0 | 18 → 44 | 7 / 44 | 724.97 → 740.17 | 784.96 → 800.15 | 26 → 23 |
| House of Laban | 1 | 22 → 34 | 16 / 34 | 672.08 → 838.54 | 732.07 → 898.53 | 22 → 31 |

Each Nephite side lost exactly one Seat, and each defeat countdown took approximately 60 seconds. The finish-time gains therefore reflect later Seat loss. First-combat rosters and peak Nephite army counts matched the corresponding controls. Worker losses are cumulative. The Lamanite slot-1 and Laban slot-0 trials recorded fewer losses even across their longer games, so they also lost fewer workers through the earlier finish. The two increases cannot be located within the common horizon from these reports.

## Defensive pressure and conversion

The pressure counter samples Age II simulation steps with fewer than four live land military units while the most recent bot decision saw a base threat. The reports provide totals. For a matched comparison through the earlier finish, the trial can contribute at most one pressure step per extra tick beyond the baseline. This gives the bounds below.

| Opponent | Nephite slot | Full-match pressure, baseline → trial (s) | Trial minus baseline through earlier finish (s) |
|---|---:|---:|---:|
| Lamanites | 0 | 129.2 → 123.5 | −8.9 to −5.7 |
| Lamanites | 1 | 131.1 → 145.5 | +2.4 to +14.4 |
| House of Laban | 0 | 211.0 → 226.2 | 0.0 to +15.2 |
| House of Laban | 1 | 229.2 → 366.1 | −29.6 to +136.9 |

At the first Teacher decision, nearby defenders stayed at five against Lamanites in slot 0, six against Laban in slot 0, and one in the blocked Laban slot-1 game. Neither slot-1 Lamanite game completed a Temple. Against Laban in slot 0, the first Teacher still arrived at 459.72 seconds and began its first channel at 564.51. The trial completed four conversions between 752.26 and 770.16 seconds, within the earlier finish at 784.96, after the trial Seat fell at 740.17. The other seven games recorded zero conversions. [Teacher gate diagnosis](../first-teacher-gates/README.md) · [Laban trial trace](trial/pair-4/v09-bot-balance-04-of-15.json)

The follow-up replay paired local defender samples before the earlier Seat loss and tracked Slinger presence and removals. [Local-defense replay](local-defense/README.md)

## Baseline reproduction

The new Laban control reproduced all 388 retained scalar values in both earlier reports. The new Lamanite control reproduced all 386 shared values in its earlier report. That report used `support_policy=enabled` and omitted two per-slot support flags added later. The comparison excluded wall-clock and step timings and observer-only telemetry, preserved array order, and compared the remaining nested values exactly. [Lamanite prior control](../support-policy/controlled/enabled/v09-bot-balance-00-of-15.json) · [Laban prior control](../first-teacher-gates/enabled/v09-bot-balance-04-of-15.json) · [Arrival-hold report](../support-policy/arrival-hold/v09-bot-balance-04-of-15.json)
