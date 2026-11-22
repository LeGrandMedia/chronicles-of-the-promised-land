# v0.7 Balance and Data Specification

This revision keeps stable unit, building, and upgrade IDs where a rule is renamed. New content remains data-driven so the runtime can read catalog values rather than infer behavior from prose.

## Religious conversion and Lamanite economy

- All religious units auto-target eligible enemies. A conversion channel takes 5 seconds and has no conversion cooldown. Any unit already converted is immune to another conversion.
- Nephite Teachers use **Soften the Heart** without research. After **Mighty Change of Heart**, an Ammonite can activate **Prostrate**: any unit that kills it during the next 5 seconds converts to an Ammonite Covenant Keeper. The ability cooldown is 12 seconds.
- Lamanite Rameumptom research IDs: `wrath_of_the_fallen` gives 5 Anger on each enemy-caused unit or structure loss; `lofty_rameumptom` triples conversion range and line of sight. The actual Lamanite building ID remains `temple_of_the_land`.
- **King Noah** uses the `religion_generation` perk on the Age-III choices for Lamanites and Laban. Zoramite Captains generate double Priestcrafter Anger (0.12/s) and grant nearby allied military units +15% attack speed within 6m. Lamanite workers display as **Idolatrous Laborer**; their stable catalog ID remains `industrious_laborer`. Lamanite faction roster omits `tower_of_watchmen`.

## Nephite and Lamanite age choices

Nephites and Lamanites each have three choices at Ages II, III, and IV. Existing costs, times, age prerequisites, and hero IDs are retained; new IDs are `lehi`, `amulek`, `teancum`, `laman_and_lemuel`, `ammoron`, and `zerahemnah` where applicable. Permanent economic choice effects use `economy_bonuses`: distinct Food, Timber, Precious Things, construction, carry, and unit-training bonuses are displayed on the choice. Religious generation choices use the `religion_generation` hero perk. Teancum’s Age-IV option uses `train_speed: 0.10`.

## Jaredite livestock and building upgrades

- Curelom unlocks in Age I; Cumelom in Age III; Elephant in Age IV. Cumeloms produce 0.3 Precious Things/s; Cureloms produce 0.4 Timber/s. Elephants produce 0.8 Cement/s, four times their former 0.2/s rate, and are limited to one per flock.
- Each Gathering of Flocks supports 10 queued/living beasts. Jaredite flock-building limit is 5; `enlarge_the_flocks` is an Age-IV upgrade that raises the building limit to 7.
- Jaredite hero choices alternate between `beast_income` and `build_speed`, with 15%, 20%, and 25% by Age II, III, and IV. Both Coriantumr and Shiz share the same tiers.
- `coriantumr_storehouse_beast_income_2/_3` and `shiz_storehouse_beast_income_2/_3` each add 20% beast income, cumulatively with existing tier 1.
- The winery’s base generation is 1 Food/s. `winery_double_food` raises it to 2 Food/s. `winery_drunkness` adds 0.5 Drunkenness/s. The stable building ID remains `winery`. `gate_segment_upgrade` is a non-building Age-II upgrade researched at `works_of_defense` for 50 Timber and 25 Precious Things; it is available to wall-building factions, not the House of Laban.
- `glowing_stones` stays the same research and keeps its existing barge-vision behavior. **Glowing Stones** is the display name of the Jaredite religious stat; each point adds 0.3% maximum hit points to buildings. It no longer adds attack damage.

Cement Towers fire at range equal to their 16m line of sight, deal 40 base damage every 4 seconds, and splash in a 2m radius for half damage. Their minimum firing range is 3m, and their attack deals 25% damage to buildings.

## Combat-age specification

No military unit is available in Age I. Weak military units unlock in Age II, most military units in Age III, and all remaining and elite military units in Age IV. The Band of Kishkumen (`gadianton_robber`) is an Age-II raider. Hero choices still advance the civilization and select the hero for that age. Cumelom and Elephant use their catalog `age` values as specified above.

## Runtime implementation

The runtime implements `economy_bonuses` keys `gather_food`, `gather_timber`, `gather_precious`, `carry_capacity`, `train_speed`, `religion_generation`, `beast_income`, and `build_speed`; Winery upgrades `winery_double_food` and `winery_drunkness`; flock cap research `enlarge_the_flocks`; elephant one-per-flock limits; military age gates; conversion immunity and the 5-second channel; Ammonite Prostrate (5-second trigger window, 12-second cooldown); and the two Rameumptom effects. `gate_segment_upgrade` toggles only the researched wall segment to a gate segment. Descriptions only summarize these effects; they are not the behavior implementation.
