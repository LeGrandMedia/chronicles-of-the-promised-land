# Emergency Slinger defense around the Seat

**Keep the priority opt-in.** The trial fielded more nearby defenders in both House of Laban starts and showed no consistent local improvement against Lamanites. All four Nephite trial sides still lost their Seat and the match. The new samples explain where the Slingers appeared and how long they survived, while the existing reports establish the match outcomes. Recommended and actual model was GPT-6.2 Sol. [Original trial analysis](../README.md) · [Comparison JSON](comparison.json)

## Matched checkpoints before Seat loss

The observer samples every 100 simulation steps, or 10 nominal game seconds, from Age II. It counts available owned defenders within 24 m of the lowest-ID living completed Seat and excludes Scouts, religious units, ships, embarked units, and disarmed units. We paired identical step numbers while both Seats stood, strictly before the earlier Seat loss. The local defender columns below use the subset after the trial's first emergency queue when both planners last observed a visible base threat. That subset includes every post-queue checkpoint in these matches.

| Opponent | Nephite slot | Checkpoints with trial more / equal / fewer nearby defenders | Mean nearby defenders, baseline → trial | Last paired checkpoint (s) | Seat HP there, baseline → trial | Worker combat losses there, baseline → trial |
|---|---:|---:|---:|---:|---:|---:|
| Lamanites | 0 | 0 / 6 / 1 | 0.29 → 0.14 | 530 | 410 → 1,117 | 18 → 22 |
| Lamanites | 1 | 1 / 7 / 1 | 0.33 → 0.33 | 460 | 148 → 1,111 | 11 → 11 |
| House of Laban | 0 | 4 / 10 / 0 | 0.29 → 0.64 | 720 | 345 → 1,623 | 16 → 17 |
| House of Laban | 1 | 12 / 5 / 0 | 0.29 → 1.59 | 670 | 137 → 565 | 14 → 13 |

At 670 seconds in the second-slot Laban comparison, the trial had four nearby defenders and 77 local HP, with three tracked Slingers within the radius. The baseline had one defender and 16 local HP. The trial still produced no Teacher attempt. A periodic roster snapshot and a Teacher queue decision can sample different times. The last paired Seat HP was higher in every trial. Across the full post-queue checkpoint window, the second-slot Laban trial's mean Seat HP was lower. [Lamanite control](baseline/pair-0/v09-bot-balance-00-of-15.json) · [Lamanite trial](trial/pair-0/v09-bot-balance-00-of-15.json) · [Laban control](baseline/pair-4/v09-bot-balance-04-of-15.json) · [Laban trial](trial/pair-4/v09-bot-balance-04-of-15.json)

## Slinger presence and removals

The observer tracked 25 newly observed owned unconverted Slingers in the full trials, with 27 emergency queues recorded independently. The lifecycle starts at the first end-of-step observation, and the report carries no queue-to-unit identity. All 15 Slingers observed before the paired cutoff were already within 24 m at their first observation. Eleven had a certified lethal-damage removal before that cutoff. Four had continuous sampled residence around the trial Seat through the cutoff. In the second-slot Laban match, one of the three Slingers observed near the Seat at 670 seconds died at 671.28, before the earlier Seat loss at 672.08.

| Opponent | Nephite slot | Earlier Seat loss (s) | Trial Slingers observed before loss | Lethal removals before loss | Still sampled near Seat through loss | Slinger unit-seconds near Seat through the last paired checkpoint |
|---|---:|---:|---:|---:|---:|---:|
| Lamanites | 0 | 532.12 | 2 | 1 | 1 | 0.7 |
| Lamanites | 1 | 460.92 | 2 | 2 | 0 | 21.3 |
| House of Laban | 0 | 724.97 | 5 | 4 | 1 | 54.0 |
| House of Laban | 1 | 672.08 | 6 | 4 | 2 | 148.3 |

Unit-seconds add each tracked unit's sampled residence, so simultaneous Slingers contribute separately. All 25 tracked Slingers eventually had certified combat deaths. Ownership changes and unknown removals had zero entries for this cohort. These records establish local presence and survival in the tested starts. They leave the relative contributions of reinforcements, worker losses, and opponent actions to overall Seat survival unresolved.

## Observer regression

All four processes exited 0 with valid reports from Windows SHA-256 `4F7777E9670E221AAF596BBD008E5231E10AC7B5359BCA8E86EAEA512A998DBD`. Both trial processes passed their two authority queue checks. Against the original experiment, exact recursive comparison matched all 1,581 and 6,683 retained scalar values for the two controls, and all 1,369 and 8,067 for the two trials. The comparison excluded `local_defense` and wall-clock and step performance timings. Existing support and Teacher traces remained in the comparison. Observer caps, time alignment, and lifecycle checks passed with zero dropped records. [Comparison JSON](comparison.json)
