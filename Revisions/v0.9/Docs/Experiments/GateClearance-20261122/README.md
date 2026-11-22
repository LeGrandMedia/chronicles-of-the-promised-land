# Archived gate-clearance experiment — not in the playable handoff

Preserved when the user ended the open-ended goal and requested final deliverables. Recommended model: GPT-6.2 Astra for coordinated geometry/navigation; root integration plus GPT-6.2 Sol reviews implemented the candidate. The source snapshot is under `candidate/`; `source-manifest.json` identifies every preserved file. It is outside Unreal's compiled Game/Source directory. Do not copy it over the final source without a new validation pass.

The experiment enlarged gates, generated passage metadata from unit bounds, used oriented body-versus-pier collision, protected entry/exit waypoints, joined adjacent same-owner gates, and added an authority-trajectory renderer. The most recent packaged candidate passed 119/120 movement cases; its one failure was an unnecessary 0.66 m parallel-route detour. A subsequent direct-clearance source fix compiled but was not packaged or regression-tested before the cutoff.

The completed rendered probe checked 90 trajectories and failed six. Rendered trajectories exposed beast-leg intersections with wall-end geometry even where authority movement succeeded. Rest-model bounds are insufficient to certify all animated poses. Also review the joined-pier part-name filter: positive-side parts use suffixed names such as gate_bastion_2, and the candidate filter only matches unsuffixed names. No visual pass or release claim is justified.

Evidence: ../../Evidence/v0.9/maps/gates-hud-opening-first (initial failures), ../../Evidence/v0.9/maps/gates-hud-refined (movement failures and partial visual timeout), and ../../Evidence/v0.9/maps/gates-hud-food-assignment (119/120 movement, full visual report). The final handoff restores the earlier 46-case gate navigation and original gate artwork, including their documented large-unit visual-clearance limitation.

Continuation should use actual animated envelopes, validate both sides of adjacent gates, and test centered/offset/reverse/retarget/blocked paths plus parallel travel and unrelated remote gates. Do not promote the candidate on movement tests alone.
