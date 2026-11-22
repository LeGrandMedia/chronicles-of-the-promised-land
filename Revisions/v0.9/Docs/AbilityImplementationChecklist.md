# v0.7 Unit and Ability Reference

This reference summarizes current v0.7 behavior. Stable IDs and canonical base statistics remain in `Game/Content/Data/catalog.json`; the native regression reports are under `Docs/Evidence/v0.7/native/`.

## Behavior requiring special attention

- **Band of Kishkumen (`gadianton_robber`):** Storehouse theft orders repeatedly steal resources. Raid move targets the first eligible enemy economic structure in sight, then returns to the nearest friendly drop-off. Raid move does not damage the structure.
- **Trader in Precious Things (`trader_in_precious_things`):** follows its player-set route. Each pass within 4m of an own worker grants 2 Precious Things, with a 30-second cooldown shared by all the owner’s traders. No market-to-market trade occurs.
- **Jaredite livestock:** Curelom produces 0.4 Timber/s; Cumelom produces 0.3 Precious Things/s; Elephant produces 0.8 Cement/s and is limited to one per flock. Cumeloms require Age III and Elephants Age IV. Beasts have their own producer, age, and construction access in the catalog.
- **Drunken Concubine:** Seduce converts eligible enemies into Henpecked Gentiles. Her 5m aura reduces movement speed 20%, attack speed 25%, and maximum HP 15%, preserving current HP fraction. The Great and Spacious Building aura radius is 8m.
- **Combat counters and conversion:** ranged units deal 50% bonus damage to religious units; melee units take 25% less ranged damage. Religious conversion takes 5 seconds against eligible targets, has no cooldown, and converted units are immune to conversion.
- **Passenger capacity:** King's Chariot 1 hero; Elephant 10; Jaredite Barge 12; Hagoth's Vessel and other boats 10. Each carrier costs 3 population.
- **Gadianton infiltration:** Plunder Share/Hoard may be researched at any Lasciviousness; their 2×/3× return effects are active only while above 30. Vision sharing is active above 50; camp deposit diversion is active above 70, with full diversion at 100. Current thresholds and transfer rules appear in the catalog metadata.

## Faction-specific text

Faction-only descriptions belong in each definition’s `faction_descriptions` map. The map keys must be present on that definition’s faction roster. Names are user-facing labels in `names.json`; mechanical IDs are unchanged by renaming. No Judgment Seat or separate Rameumptom definition is part of the current catalog.


