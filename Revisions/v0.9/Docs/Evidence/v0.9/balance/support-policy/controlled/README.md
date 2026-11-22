# Same-executable support-order comparison

Two mirrored faction pairs ran with and without bot support orders using the same packaged Windows Development executable, SHA-256 `9F4710F2BD828F05664D49FFB4A01498886EC486BE8F88220919E8F1B1D25257`. Both modes used Normal bots, fog, ordinary starting resources, the stock Zarahemla Valley map, and a 12,000-step match limit. All four shard processes exited 0 with the expected policy labels and all eight matches were valid. The executable hash stayed fixed throughout. The enabled run reproduced the player telemetry and finish times of the initial support replay exactly. [Initial support replay](../README.md)

Run the inner executable with `-RTSV09BotBalance -RTSBalanceShards=15 -RTSBalanceShard=0 -nullrhi -unattended -nosound` for the Lamanite pair. Use shard 4 for House of Laban. Add `-RTSV09BotSupportBaseline` to skip the new support planner while leaving ordinary idle conversion, religious auras, and the production and combat rules in place. The baseline switch requires balance mode in a non-Shipping build. The JSON field `support_policy` identifies the mode. Archive each report before rerunning its shard number. [Lamanites baseline](baseline/v09-bot-balance-00-of-15.json) · [Lamanites enabled](enabled/v09-bot-balance-00-of-15.json) · [House of Laban baseline](baseline/v09-bot-balance-04-of-15.json) · [House of Laban enabled](enabled/v09-bot-balance-04-of-15.json)

Each opponent defeated the Nephites in both slot assignments in both modes. First contact was identical, with no converter on either side at first contact in any game.

| Opponent | Nephite slot | First combat (s) | Match finish baseline → enabled (s) | First Nephite Teacher baseline → enabled (s) | First Nephite channel baseline → enabled (s) | Completed Nephite conversions baseline → enabled |
|---|---:|---:|---:|---:|---:|---:|
| Lamanites | 0 | 320.5 | 594.4 → 592.1 | 436.3 → 436.3 | 489.9 → — | 0 → 0 |
| Lamanites | 1 | 309.5 | 520.9 → 520.9 | — → — | — → — | 0 → 0 |
| House of Laban | 0 | 346.8 | 1,108.4 → 785.0 | 459.7 → 459.7 | 569.6 → 564.4 | 9 → 0 |
| House of Laban | 1 | 331.1 | 769.5 → 732.1 | 555.4 → — | 555.5 → — | 1 → 0 |

All other sides completed zero conversions in both modes. First religious-building times matched in all four starting-slot assignments. When House of Laban started in slot 0, the Nephite Temple finished at 511.9 seconds in both modes. The Nephites produced a Teacher by 555.4 seconds in the baseline and never did with support enabled. The Nephites reached the same peak army unit counts in both modes. Peak opposing army unit counts were 41 to 40 and 34 to 34 for Lamanites and 16 to 15 and 18 to 17 for House of Laban. The full reports include religious-resource totals and army population.

The 7,300-step baseline cutoff found zero conversions in Nephite slot 0 and one in slot 1 by 729.97 seconds. The slot-0 baseline still had zero conversions on the same 7,850th step that ended the support-enabled game. All nine later slot-0 baseline conversions therefore occurred after the support-enabled game had ended. The slot-1 baseline conversion occurred before the earlier support-enabled defeat at 732.1 seconds. In that assignment, the enabled Nephite planner had no religious unit to move. [7,300-step baseline](early-baseline/v09-bot-balance-04-of-15.json) · [7,850-step baseline](slot-zero-horizon/v09-bot-balance-04-of-15.json)

The test changes support behavior for both sides together. It establishes that enabling those orders shortened the two House of Laban wins and reduced lifetime Nephite conversions in these scenarios. The slot-0 conversion difference arose after the earlier finish. The slot-1 difference includes a change in Teacher production before there could be any Teacher support orders. Step timings were recorded while pairs ran concurrently and another VM was active.

The subsequent per-slot trace isolates the two factions' support orders and records the arrival transition investigated after this run. [Per-slot order trace](../role-trace/README.md)
