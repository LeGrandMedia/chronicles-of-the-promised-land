# Decisions and unresolved interpretations

October 20, 2026. Actual executor: Codex (GPT-6-based per session instructions; exact SKU/setting not exposed). Assigned task tier Sol. No tiers escalated or rewritten.

| Record | Decision / authority | Impact |
| --- | --- | --- |
| SESSION-01 | Current user explicitly requests all Sol assignments except prerequisite bottlenecks, plus a summary in the Obsidian vault. | Complete independent administration; do not implement Astra architecture simply to manufacture a passed dependency. |
| SESSION-02 | Create bookkeeping and execution records early, before the packaged-game gates. Reversible organizational choice by executor. | A011/A017 administrative outputs can be verified now; neither Phase 01 nor Phase 02 is declared passed. |
| SESSION-03 | Perform source-only audio extraction to reduce later repetitive work, retain Uxx asset-production identifiers and null runtime IDs. | A044 runtime schema remains an Astra task; A051/A185/A186/A194 remain unchecked. No new gameplay IDs or classification decisions frozen. |
| SESSION-04 | Establish ledgers with zero session spend and no acquisitions; unknown prior external expense remains unreconciled. | Do not spend the displayed remainder without reconciliation, concrete sample evidence and explicit authorization. |

Confirmed direction is in ProductContract.md. The following D01–D16 remain open under A004 (Astra Heavy); the original proposed resolutions remain in Development Findings and Decisions. No proposal below is silently adopted.

| ID | Unresolved subject | Owner / next action |
| --- | --- | --- |
| D01 | Release player count/maps/OS, including 1v1/2v2 proposal | User for scope; implementing agent for measured feasibility, A003/A004/A007/A041 |
| D02 | Footprints, work positions, construction/access ranges | Implementing agent prototypes before placement/navigation, A004 |
| D03 | Rounding/tick order/fractional repair payments | Astra architecture/rules work, A004/A024/A077 |
| D04 | Projectiles, LOS loss and Bowman profile | Implementing agent before combat; user for changed mechanics |
| D05 | Gate passage, wall platforms, Samuel ejection | Implementing agent before wall/hero integration |
| D06 | Headquarters foundation/timer semantics | Implementing agent records interpretation before A204 |
| D07 | Shared allied vision and stale static markers | Confirm competitive expectations before A030/A108 |
| D08 | Local draft detection trust boundary | Preserve private local drafts; record limitations before A164; stricter policy requires user decision |
| D09 | Transport eligibility, passenger abilities and blocked ejection | Resolve before transport implementation |
| D10 | Lamoni training rate, aura categories, Ammon sling | Resolve before hero implementation |
| D11 | Stripling suppression and Shiz last-attack ordering | Record explicit interpretation before survival abilities |
| D12 | Mixed cargo/crate resource priority and capacity | Resolve before crate/plunder implementation |
| D13 | Saves, reconnect reservations, departed-player handling/host loss | Confirm expectations before persistence; dedicated release decision remains A221 |
| D14 | Misplaced Lamanite Rameumptom sentence and index acquisition routes | Follow governing faction restriction; verify in reviewed mapping A043, no new cross-faction access |
| D15 | Physical hidden blockers and generic feedback | Record/test constraint during placement and visibility work |
| D16 | Deferred general capture/diplomacy and inert lifecycle hooks | Keep general systems out of initial exposed commands; record in A004 |

Outstanding user actions are deliberately deferred to the relevant concrete milestone: release-scope confirmation, rig/style review, pronunciation/performance review, unfamiliar tutorial reader, dedicated hosting decision, justified purchase, private Steam onboarding and final publication instruction. None is needed to verify the administrative work completed in this session.

## Astra Light session — October 20, 2026

User authorized Astra Light tasks except heavier-tier bottlenecks and a vault summary. A001/A003 completed; see ScopeRecord.md. User answered "Keep the proposed target" for provisional 1v1/2v2, 200 population per player, one 1v1 land map, one 2v2 land map and one water map. D01's provisional preference is resolved; measured feasibility and release freeze remain open. Do not ask again for the same provisional preference. All other D records remain pending A004 at their stated dependency deadlines. Respect phase ordering; next work is the Heavy decision pass, then remaining Phase 01 tasks. Full 131-task audit: AstraLightTaskStatus.json.

## Playable LAN implementation — October 20, 2026

The latest user instruction authorizes building the game and making reasonable assumptions without additional questions. Earlier administrative-only restrictions above describe previous work. The current artifact is a Windows Unreal 5.3 Development package, with all source retained under `Game/` and reproducible build/package scripts under `Tools/`.

- The host runs the authoritative 0.1-second simulation. Clients submit validated orders. Only visible enemy entities are included in client state; allied vision is shared. Snapshots publish four times per second in reliable chunks of at most 96 entities, reassembled by sequence before presentation. A four-process 800-unit snapshot test exposed dropped unreliable chunks under burst saturation; reliable chunks and an explicit 500,000-byte/s per-client LAN budget resolved the observed freeze.
- The Canvas isometric presentation uses original procedural shapes and synthesized sound cues. It is playable presentation for this build, not completion of the painterly art and recorded-voice production plan.
- Unit jobs, beast evolution, and conversion preserve identity. Research, production reservations, refunds, faction costs, caps, and headquarters victory follow the executable catalog and scenario checks. Fixed-step ordering is entity ID order; conversion and production update population immediately.
- Map footprints, nearby wall platforms, shoreline transport, placement snapping, and static navigation grids are concrete v0.1 implementation choices. Obstacle caching reduces the measured full-army simulation cost without changing collision rules. There is no opposing computer AI in the sandbox.
- Loss of every completed headquarters starts a 60-second defeat countdown; a foundation pauses it only while construction progressed within the past 15 seconds. Defeat removes the defeated player's entities; allies may continue. A disconnected player has 60 seconds before defeat. Host loss ends the session, with a menu error on clients. Saved-game and reconnect flows remain absent.
- Draft Liahona checks remain local with a server-side submitted-message check. This trusted-LAN build does not add anticheat, Steam services, matchmaking, or a public dedicated service.
- The gameplay and four-process loopback checks pass. Physical-PC LAN testing and rendered GPU performance remain unverified. Complete evidence and precise limitations are in `RuntimeBuildReport.md` and `KnownIssues.md`; historical phase-gate checkboxes are not automatically promoted.
- Menial catalog, labeling, packaging-support, and test-fixture work was delegated to gpt-6-luna; client/presentation work used gpt-6.1-sol. Each of the 407 original tasks now carries a model/effort suitability label; labels do not claim historical execution by that model. No assets were purchased.
