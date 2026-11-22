# Teacher and Concubine order comparison before the arrival fix

Four balance runs compared both faction planners on and off independently. Each run played both Nephite and House of Laban starting-slot assignments from the stock Zarahemla Valley layout on Normal with fog. The same Development executable, SHA-256 `7CD832201BF0B0686D488CA52D6EF3171620C42DD8DCD20825DE529958497BF9`, ran all eight valid matches. Each process exited 0 and reported its requested policy.

| Nephite slot | Nephite support orders | Laban support orders | Result | Finish (s) | Nephite Seat loss (s) | First Teacher (s) | First completed Nephite conversion (s) | Nephite conversions |
|---|---|---|---|---:|---:|---:|---:|---:|
| 0 | Off | Off | Laban win | 1,108.4 | 1,048.4 | 459.7 | 991.0 | 9 |
| 0 | On | Off | Time limit | 1,199.9 | 1,189.2 | 459.7 | 629.9 | 6 |
| 0 | Off | On | Laban win | 789.7 | 729.7 | 459.7 | — | 0 |
| 0 | On | On | Laban win | 785.0 | 725.0 | 459.7 | — | 0 |
| 1 | Off | Off | Laban win | 769.5 | 709.5 | 555.4 | 560.4 | 1 |
| 1 | On | Off | Laban win | 769.5 | 709.5 | 555.4 | 560.4 | 1 |
| 1 | Off | On | Laban win | 732.1 | 672.1 | — | — | 0 |
| 1 | On | On | Laban win | 732.1 | 672.1 | — | — | 0 |

With Laban support off and Nephite support on, the slot-0 Teachers completed four conversions by 704.8 seconds. The slot-0 baseline had no conversions by 785.0 seconds and its first completion was at 991.0. The Nephite Seat lasted 140.8 seconds longer with its support orders enabled, and the match reached the time limit during the defeat countdown. Teacher traces captured resets when targets left range or line of sight, or disappeared, and later completions. [Lifetime and cutoff comparison](../controlled/README.md) · [Slot 1 baseline trace](slot_1_baseline/v09-bot-balance-04-of-15.json) · [Both baseline trace](baseline/v09-bot-balance-04-of-15.json)

Enabling Laban support alone brought its slot-1 win within 4.7 seconds of the both-on result in that starting-slot assignment. With Laban in slot 0, enabling its orders yielded the same 732.1-second win and prevented a Teacher from reaching the field with either Nephite policy. The Nephite Temple completed at 511.9 seconds in all four slot-1 starts, and training was accepted at 513.4 seconds in the two with Laban support off. The logs contain no Teacher training attempt in those two starts with Laban support on. With a Teacher present, its first channel completed at 560.4 seconds, and its next channel reset with incoming damage before the unit died at 564.7 seconds. [Slot 0 baseline trace](slot_0_baseline/v09-bot-balance-04-of-15.json) · [Slot 1 baseline production log](slot_1_baseline/v09-bot-balance-04-of-15.log) · [Both baseline production log](baseline/v09-bot-balance-04-of-15.log)

The both-on slot-0 trace exposed an unsafe transition between bot decisions. The Teacher arrived at a movement rally and became idle at 564.31 seconds. An innate conversion channel started at 564.41 with the rally unchanged. Damage reset the channel at 566.11 and 566.31, and the Teacher died at 566.51. This led to queuing a hold after bot-issued support movement so the next bot decision can check the new position. [Both enabled trace](enabled/v09-bot-balance-04-of-15.json)

Run shard 4 with `-RTSV09BotBalance -RTSBalanceShards=15 -RTSBalanceShard=4 -RTSV09BotSupportTrace -nullrhi -unattended -nosound`. Add `-RTSV09BotSupportBaselineSlot=0` or `-RTSV09BotSupportBaselineSlot=1` for the mixed modes, and `-RTSV09BotSupportBaseline` to bypass both. These optional non-Shipping balance flags do not change player orders. The observer reads authoritative end-of-step state for diagnostics. Where a prior target exists, the trace's `observed_target_*` fields refer to that target. Each side can record up to 192 events with a count of dropped events and up to 40 actual support-training attempts in the log. No Nephite events were dropped. The two slot-0 Nephite games with Laban support enabled dropped 108 and 92 Laban events due to the cap. Process timings include concurrent workload.
