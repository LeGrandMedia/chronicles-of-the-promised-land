# AI handoff

Read README.md, then the vault's `01 Foundation/Design Contract.md`, `08 Development Plan/Developer Handoff.md`, and `07 Reference/Data and Assets.md`. Use paths relative to this checkout. Current implementation: `Revisions/v0.8`. All 21 requests and eight cleanup gaps in that release are completed; do not replay them as unfinished work. Read only task-relevant notes.

Follow the current user's requests. Preserve playtest copies when starting another revision. Keep stable catalog IDs; use names.json for labels and visuals.json for replaceable assets. Preserve the host-authoritative simulation, validation of client orders, fog/privacy filtering, and atomic resources and ownership changes. Game source is under `Game/Source/Chronicles`, data under `Game/Content/Data`.

Label concrete tasks with their best-suited model. Delegate routine catalog, inventory, documentation, and coverage work to GPT-6 Luna; gameplay/UI/network integration to GPT-6.1 Sol; difficult architecture to GPT-6 Astra, or available equivalents. Distinguish the recommended model from actual execution.

Build/package and run relevant checks after changes, then update affected canonical notes. Existing evidence applies to the delivered executable, not future edits. The producer-only finalization script and old private workspace history are intentionally omitted. Revisions/v0.7/Docs is historical reference, not another source baseline. Public copies omit local account/session settings and raw machine logs; asset prompts/hashes remain.
