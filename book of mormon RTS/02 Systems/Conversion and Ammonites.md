# Conversion and Ammonites

Every faction has religious conversion. Idle religious units automatically acquire eligible nearby enemies; right-click uses conversion. All channels require five uninterrupted seconds, with no success/retry cooldown. Movement, damage, death or loss of sight/range resets the channel; target damage alone does not. One effective converter per target; show progress to both owners.

## Eligibility and ownership

Eligible: hostile living biological foot units, including workers, infantry, captains and religious units. Exclude allies, neutrals, buildings, heroes, beasts, carriers and embarked units. Any previously converted unit is permanently immune, regardless of later ownership. Conversion requires 60 seconds of hostility between owners.

Preserve entity identity and health fraction; clear old buffs/orders and drop cargo as a crate; transfer ownership/population atomically and apply new-owner HP rules. Require enough population and Ammonite capacity both when a channel begins and when it completes; concurrent channels cannot overbook. No resource fee or queue. Religious rewards/losses: [[Favor of the Lord]].

| Converter | Result |
| --- | --- |
| Nephite Teacher, **Soften the Heart** | Ammonite Covenant Keeper; innate, no research |
| Lamanite Priestcrafter, **Stiffen the Neck** | Zoramite Captain |
| Upgraded Lamanite Rameumptom | Zoramite Captain; autonomous defensive channel |
| Laban Drunken Concubine, **Seduce** | Henpecked Gentile (ordinary Laban worker) |
| Gadianton Lawyer, **Bribe** | Retains class and former-owner appearance; using Bribe reveals the Lawyer from stealth during the channel. [[Gadianton Plunder and Stealth]] |
| Jaredite Oath Keeper | Retains class |

## Ammonites

Conversion-only nonviolent elite support, inspired by Alma 24:16–19; Stripling Warriors are a separate trained troop. No recruitment button, attacks, construction, reconversion, job-class recovery or further conversion. Heal one ally or gather, never simultaneously; strongest healer only. Current stats/rates/population: [[Data and Assets]]. Maximum 12 owned; require capacity at channel start and completion, leaving the target unchanged if completion cannot fit.

**Mighty Change of Heart** grants **Prostrate**, not basic conversion. A 12-second cooldown activates a five-second window: a unit killing the Ammonite becomes an Ammonite. Converted killers remain immune. The dead Ammonite stays dead. Intended wording covers any killing unit; v0.7 allows otherwise ineligible attacking heroes/beasts/carriers for martyrdom, but excludes buildings and previously converted killers. Document that exception deliberately when reconciling rules.

The v0.8 hostility guard and capacity checks passed native validation. Soften the Heart remains innate. See [[Validation Report]].
