# Development Action Plan

The playable game exists; completed scaffolding and repeated per-asset tasks are retired. Retained goals below are separate from the current request. Resolve dependencies when taking up a goal without reopening obsolete phase gates.

| Work | Best-suited model | Required evidence |
|---|---|---|
| Human balance and faction playtests | GPT-6.1 Sol | Test real faction openings, matchups, and combat balance with human players; focused v0.8 implementation checks are already recorded in [[Validation Report]]. Preserve explicit asymmetry. |
| Prove multiplayer beyond localhost | GPT-6.1 Sol | Physical two-PC LAN, land/water, 1v1/2v2, disconnect/host loss, version mismatch, latency/loss/replay checks; actual connection/firewall guidance. |
| Measure scale/stability | GPT-6 Astra | Named hardware; target 60 FPS at 1080p and simulation below 100ms. Separate CPU/GPU/animation/memory/bandwidth measurements at 200/400/800 moving units; economy/fights, soak and repeated lobby cycles. Profile before architecture/capacity changes. |
| Finish presentation/accessibility | GPT-6.1 Sol | Six-faction readability, rebinding, subtitles/voice volume, reduced motion and non-color-only team cues; action states, construction/destruction, hidden effects and normal-zoom art reviewed. [[Data and Assets]]. |
| Produce audio/provenance records | GPT-6 Luna | [[Voice Lines]] mapped to licensed/original recordings or explicit text fallbacks; creature/world/UI/music cues, anti-spam, loudness and hidden-event privacy checked; authorized budget respected. |
| Finish maps/onboarding | GPT-6.1 Sol | Viable six-faction starts on 1v1 land, 2v2 land and water; unloading, raid routes/congestion checked. Scripted training teaches controls and faction mechanics without requiring opponent AI. |
| Private save/reconnect, when scoped | GPT-6 Astra | Versioned atomic IDs, queues, reservations, cargo, religion, cooldown/life flags and Liahona. Authenticated resync exposes only entitled state; restore active mechanics without duplicate commands. Host migration separate. |
| Steam/internet and optional dedicated hosting | GPT-6 Astra | Retain LAN. Verify current official integration; two accounts/PCs on separate networks; invitations, lobbies, transport/version checks, clean install/update/rollback. Shared authority; headless server only if scoped. Account/cost/publishing authorization stays separate. |
| Public release candidate | GPT-6.1 Sol | Exact build/hash; distinct and mirror matchups, human playtests, exploit/clean-machine/accessibility checks, realistic hardware/network claims, rights/AI disclosure, credits/store media, rollback/support plan. User approval of concrete candidate before publication. |

Delegate routine scripts, receipts, inventories and report assembly within these tasks to GPT-6 Luna. Creative expansion: [[Deferred Ideas]]. Current evidence: [[Validation Report]].
