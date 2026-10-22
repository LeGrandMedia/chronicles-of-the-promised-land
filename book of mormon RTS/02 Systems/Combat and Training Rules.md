# Combat and Training Rules

Common simulation contract; see [[Design Contract]] for authority and [[Data and Assets]] for entity values. Use seconds, meters, HP, flat armor and deterministic player-owned state on a 0.1-second tick.

## Combat

- Damage is per hit; interval is seconds between hits. Final damage is `max(1, floor(attack × applicable multipliers) − armor)`. Zero-attack units cannot attack or receive this minimum. No accuracy RNG or friendly fire.
- Ranged attacks deal reduced damage to buildings; melee demolition bonuses belong to unit data. Counters favor ranged against religious units and melee against ranged. The later universal conversion rule in [[Conversion and Ammonites]] supersedes conversion-speed counters.
- Strongest attack aura, strongest armor aura and strongest healer apply independently; identical sources do not stack. Permanent upgrades and faction multipliers remain independent. No ordinary passive regeneration except explicit abilities and Jaredite flock healing.
- HP changes preserve fractional current-health ratio; never resurrect, erase damage or grant rounding heals. HUD rounds live HP upward. [[Favor of the Lord]] defines religious rescaling.
- Support units answer attack orders with their documented support action or contextual refusal. Workers do not automatically fight like military units; military auto-acquisition skips workers and passive noncombatants. Attack Move is available to units, not structures. Stop, hold, patrol and move remain available where applicable.

## Production and progression

- Population cap 200; queues immediately reserve resources and population. One queue per producer; research once per player, never queued twice. Require completed owned prerequisites. A finished unit waits in queue if no legal spawn space exists.
- Cancel units/research for a full refund, including active jobs. Cancel a foundation for the unbuilt fraction of paid resources; destroyed foundations refund nothing. Producer destruction loses its active job and fully refunds waiting jobs. No general structure capture system is exposed.
- Normal new-unit cost/time modifiers: Nephites 1.25×/1.20×; Lamanites 0.65×/0.60×; others 1×, rounded up per nonzero resource. Include workers, religious units, heroes and ships; exclude buildings, research, age choices, conversions and worker job changes.
- Ages I–IV: no military in I; weak units in II, most in III, remaining/elite in IV. Locked unit and building buttons show the required Roman numeral. Chief Captains and King's Chariots unlock in III; One of the Fifty unlocks in IV. Exact gates are catalog fields. Encampment of War begins in II. Builder beasts have separate economic gates.
- Age advancement selects a hero/permanent perk and grants that hero. Only that chosen hero can be retrained after death; no free-standing initial hero recruitment. One alive/queued copy per named hero; original death cooldown is 90 seconds and life-specific bonuses reset. See [[Upgrade Catalog]].
- Laborer, Tiller, Keeper and Miner are job states of one worker, preserving identity, cargo and damage; not four recruitment buttons.

Support queued orders, economic/military rallies, shift-queueing and a two-second hold to delete owned entities. Death animations play for units; destruction animations play for buildings. Passenger capacity is separate from carrier population; limits belong with [[Data and Assets]]. Cargo loss: [[Resource System]]. v0.8 native and rendered-input checks passed; see [[Validation Report]].

Saved-game/reconnect persistence remains unimplemented; see [[Developer Handoff]].
