# Display names and stable IDs — v0.4

The catalog's `id` values are mechanical identifiers. They are used by faction rosters, production, prerequisites, visual manifests, hotkeys, and save/network data. Change a label in `names.json` without renaming the catalog ID.

The top-level `names` object contains default keys such as `definition.gadianton_robber`, `definition.winery`, and `resource.cement`. A faction-specific key uses `faction.<faction-id>.definition.<definition-id>`; for example, `faction.laban.definition.industrious_laborer` displays **Henpecked Gentile**, while the shared stable unit ID remains `industrious_laborer`. Similar overrides name Laban's Seat of the People **Laban's Crib**, Fields of Grain **Vineyards**, and Temple of the Land **Great and Spacious Building**. Lamanites display Temple of the Land as **Rameumptom**.

The Gadianton worker displays **Conspiring Civilian** and `gadianton_robber` displays **Band of Kishkumen**. Lamanite Priestcrafters continue to use their stable unit ID, and no separate Rameumptom building ID exists. Cement is an independent resource label. Resource labels also cover the faction statistics: Favor, Anger, Lasciviousness, Drunkenness, and Dominion.

The `originals` object retains default/original wording for replacing names inside older help and tooltip text. Do not use those original labels as rules or production IDs. If you add a catalog definition, add its `definition.<id>` key and optional ability label. The runtime should fall back to the catalog's `name` when a label is missing.

Faction age-choice hero labels are defined with the same stable unit IDs. Each faction has two options at each of ages II, III, and IV. Narrative eras are a game progression choice and do not imply exact historical chronology for House of Laban or shared Jaredite founders.
