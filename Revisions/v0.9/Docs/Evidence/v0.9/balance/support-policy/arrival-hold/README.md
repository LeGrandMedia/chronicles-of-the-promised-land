# Holding after support movement

The packaged Windows executable, SHA-256 `7DA5726001DD599F4E48DCFC4CD3D1B9E4F8AE15C9403DDA5D301838CF0B1016`, passed all 45 native bot checks including the two added for the arrival fix. One fixture advances a bot Teacher to its rally point with an eligible enemy already in range. The Teacher holds until the next decision and then can channel a safe replacement target. The other fixture verifies that an existing untagged queued hold and its current movement order survive a bot decision. [Native checks](../../../bots/support-arrival-hold/v09-bot-tests.json) · [Native test log](../../../bots/support-arrival-hold/Bots-v09-support-arrival-hold.log)

The same executable repeated the two Normal Nephite versus House of Laban matches with both support planners active and tracing enabled. Both matches were valid and the process exited 0. In the slot-0 Nephite game, the same Teacher arrived at the same point as in the earlier replay. The trace shows the one-step change in how its next conversion began. [Current trace](v09-bot-balance-04-of-15.json) · [Current log](v09-bot-balance-04-of-15.log) · [Prior trace](../role-trace/enabled/v09-bot-balance-04-of-15.json)

| Game time (s) | Before the arrival fix | After the arrival fix |
|---:|---|---|
| 564.31 | Teacher arrived and became idle | Teacher arrived and executed its queued hold |
| 564.41 | Idle conversion began with the movement rally still set | Teacher continued holding |
| 564.51 | Conversion already underway | Next bot decision issued conversion explicitly |
| 566.51 | Teacher died after damage interruptions | Teacher died after damage interruptions |

The slot-0 trace recorded 14 Teacher move-to-hold transitions. Laban won both matches at 785.0 and 732.1 seconds with zero completed conversions on either side. The match steps, winners, and attack counts matched the earlier both-enabled replay. The trace recorded at most 192 events per side, with 48 Laban events beyond that cap in the slot-0 Nephite game. [Earlier per-slot results](../role-trace/README.md)
