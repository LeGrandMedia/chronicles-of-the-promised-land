# v0.9 support-unit balance replay

The packaged Windows Development build ran both starting-slot assignments for Nephites against Lamanites and against House of Laban. The two processes used `-RTSV09BotBalance -RTSBalanceShards=15` with `-RTSBalanceShard=0` and `-RTSBalanceShard=4`. All four matches were valid and reached competitive victory. Both processes exited 0. These were Normal bots on Zarahemla Valley, with fog, ordinary starting resources, and the 1,200-game-second limit. The inner executable SHA-256 was `733DEA1A502E5001DDF5C205DA22163BA017DD1A1017E59871544403466A9B35`. [Lamanite matchup report](v09-bot-balance-00-of-15.json) · [Lamanite matchup log](v09-bot-balance-00-of-15.log) · [Laban matchup report](v09-bot-balance-04-of-15.json) · [Laban matchup log](v09-bot-balance-04-of-15.log)

| First slot | Second slot | First battle contact (s) | Winner | Finish (s) |
|---|---|---:|---|---:|
| Nephites | Lamanites | 320.5 | Lamanites | 592.1 |
| Lamanites | Nephites | 309.5 | Lamanites | 520.9 |
| Nephites | House of Laban | 346.8 | House of Laban | 785.0 |
| House of Laban | Nephites | 331.1 | House of Laban | 732.1 |

| Match | Side | First completed religious building (s) | First converter (s) | First active channel (s) | Peak army units / population | Final religious resource |
|---|---|---:|---:|---:|---:|---:|
| Nephites start first vs. Lamanites | Nephites | 394.0 | 436.3 | — | 7 / 10 | 22.78 |
| Nephites start first vs. Lamanites | Lamanites | 345.1 | 431.8 | — | 40 / 43 | 38.78 |
| Lamanites start first | Lamanites | 345.8 | 378.0 | — | 34 / 37 | 36.11 |
| Lamanites start first | Nephites | — | — | — | 5 / 8 | 23.32 |
| Nephites start first vs. Laban | Nephites | 417.6 | 459.7 | 564.4 | 10 / 13 | 39.67 |
| Nephites start first vs. Laban | House of Laban | 355.1 | 391.6 | — | 15 / 21 | 58.57 |
| Laban starts first | House of Laban | 300.0 | 335.6 | — | 17 / 23 | 58.57 |
| Laban starts first | Nephites | 511.9 | — | — | 8 / 11 | 21.32 |

Every side recorded zero conversions gained and zero units converted away. Every first battle contact occurred before either side fielded a converter. A Nephite Teacher started a channel in one Laban leg at 564.4 seconds, and both Nephite second-slot openings ended before a Teacher trained. The first religious buildings and converters observed within the prior 480-second controls appeared at the same times in this run. The isolated fixtures separately verified a completed Teacher conversion, a channel retained across a bot decision, a Priestcrafter occupying the Rameumptom, and Concubine laborer buffs. [480-second Lamanite control](../temple-control-pair-00.json) · [480-second Laban control](../temple-control-pair-04.json) · [Support fixture results](../../bots/support-first/v09-bot-tests.json)

All four winners match the older full round robin. Its Nephite bots completed nine and one conversions in the two Laban games, which ended at 1,108.4 and 769.5 seconds. The current games ended at 785.0 and 732.1 seconds. The executable changed across these runs and the games lasted different lengths, so the conversion difference cannot be attributed to support orders alone. The four current games provide no evidence of improved Nephite conversion or matchup balance. The logs and reports include step timings from two concurrent game processes while a VM was active, which limits performance comparisons. [Older Lamanite round-robin shard](../v09-bot-balance-00-of-05.json) · [Older Laban round-robin shard](../v09-bot-balance-04-of-05.json)
