# Developer Handoff

Read [[Design Contract]], [[Data and Assets]], then only task-relevant notes. Preserve playtest copies; work in the selected revision. Retired phase gates and the 407-action plan are not a current execution schedule.

Current source baseline: `Revisions/v0.8`; Unreal 5.3 Windows C++. Launch the revision's `Play.cmd`. All 21 latest requests and eight recovered design gaps are implemented. The packaged build passed 693 native assertions, 67 rendered-input checks at each of two resolutions, and four-process localhost multiplayer; see [[Validation Report]]. v0.7 results remain historical. Physical multi-PC LAN, Steam/internet, save/reconnect, and human balance remain unverified or absent.

## Work loop

1. Inspect actual files/instructions; distinguish user intent, implementation and evidence.
2. Complete the requested scope with reasonable assumptions; preserve unrelated work and settled decisions.
3. Delegate routine catalog/inventory/documentation/coverage to **GPT-6 Luna**; gameplay/UI/network integration to **GPT-6.1 Sol**; unresolved architecture or difficult cross-system faults to **GPT-6 Astra**. Label each concrete task with its best-suited model; distinguish actual executor where useful.
4. Verify affected behavior with appropriate native, input/render and network checks. Inspect real artifacts, state limitations, and update affected canonical notes with evidence links instead of logs.

Keep the 0.1s host-authoritative simulation independent of rendering, validate client orders, filter hidden snapshot data, preserve stable IDs, and make resource/ownership transitions atomic. Maintain reproducible schema/data checks and recoverable source/build history.

Historical vault import/preparation/difficulty generators assume the old note layout. Update their inputs before reuse; never regenerate current gameplay from historical planning data.

Next work: human balance/playtesting and physical multi-PC LAN verification when scoped. Save/reconnect, Steam/internet and production presentation stay in [[Development Action Plan]] and [[Deferred Ideas]].
