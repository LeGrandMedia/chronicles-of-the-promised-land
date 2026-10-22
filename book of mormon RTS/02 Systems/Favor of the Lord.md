# Favor of the Lord

Owns shared religious-stat rules and Nephite HP intent. Stats are visible, faction-specific, clamped to 0–100 and start at 20; never spendable. Religious structures/units generate them. Nonlinear decay makes values above 70 difficult to sustain. Hover shows counts, generation, decay and actual bonuses.

| Faction | Stat and effect |
| --- | --- |
| [[Nephites]] | **Favor of the Lord:** every owned unit's max HP, including workers, ships, heroes and Ammonites; never buildings |
| [[Lamanites]] | **Anger:** gathering, attack and training speed; Zoramite Captains generate twice Priestcrafter Anger and buff nearby military attack speed |
| [[House of Laban]] | **Drunkenness:** One of the Fifty melee splash; Winery generates 0.5/s, Fermented Reserves adds 20%, and the Great and Spacious Building generates five times the Winery rate. Concubines and the building weaken enemies |
| [[Gadianton Robbers]] | **Lasciviousness:** research-gated raid/vision/deposit thresholds in [[Gadianton Plunder and Stealth]]; lodges generate until scouted |
| [[Coriantumr Jaredites]], [[Shiz Jaredites]] | **Glowing Stones:** building HP only, no attack damage; Oath Keepers convert and generate it |

Current generation/effect coefficients: [[Data and Assets]]. Runtime decay per second is `0.025 + 0.0004 × max(0, stat − 20)²`, shared by all factions. King Noah's choice boosts religious-unit generation; Fermented Reserves increases Winery Drunkenness generation by 20%. Scouted Gadianton lodges stop generating permanently.

## Event changes

- Successful conversion: +10 new owner; old owner loses 5, or 10 for Gadianton. Owners must have been hostile for 60 seconds; check capacity at channel start and completion. Permanent conversion immunity replaces repeated-conversion farming; retire +3 / +9-per-minute conversion awards.
- Nephites lose Favor when killing workers or economic buildings, regardless of direct targeting; supersedes targeted-attack-only penalties. Preserve original noncombatant protection intent; v0.7's worker classification also includes Ammonites.
- Lamanite **Wrath of the Fallen**: +5 Anger per enemy-caused own unit/structure loss. **Lofty Rameumptom** triples that building's sight/conversion range.
- Retained Nephite awards: each 100 genuinely gathered Food delivered +1, capped +3 per rolling minute; enemy combatant killed within 15m of completed owned HQ +1, capped +3 per rolling minute. Refunds, grants, trades and stolen/transferred cargo never count as gathered Food.
- Retained penalties: completed Temple lost to enemy −10; attack declared truce −25 once per truce, disabled while diplomacy is absent. Cancel/demolition does not count as Temple loss. [[Liahona Minimap]] owns profanity penalties. Pious chat grants nothing.

## Nephite HP curve

The v0.8 implementation uses the retained dramatic curve. For post-upgrade base HP `B`, max HP is:

- `round(1 + (B − 1) × (F/50)²)` for `0 ≤ F ≤ 50`.
- `round(B × 100^((F − 50)/50))` for `50 < F ≤ 100`.
- Clamp ≥1: `F=0 → 1 HP`, `F=50 → B`, `F=75 → 10B`, `F=100 → 100B`.

Preserve fractional health ratio; no spontaneous deaths, rounding heals or resurrection. Show base/effective HP and causes; warn of population-wide HP loss.

At Favor 0/50/75/100 this yields 1 HP, `B`, `10B`, and `100B`. Fractional health is preserved. v0.8 native validation passed; see [[Validation Report]]. Start 20 and nonlinear decay remain current rules.
