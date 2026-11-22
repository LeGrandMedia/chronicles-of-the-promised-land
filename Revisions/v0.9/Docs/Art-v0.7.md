# v0.7 Art Update

Thirteen original transparent PNGs were generated with the built-in image generation tool: full-body portraits for Lehi, Amulek, and Teancum, plus 10 new research icons. The portraits and icons follow the project’s faceted low-poly toon treatment, warm light, and clean silhouettes. Each source prompt and generated-file path is recorded in `Game/Content/Data/Art/production/{id}.json`.

The visuals manifest adds sprite and unit-card entries for the three heroes, six age-choice icon aliases that reuse their selected hero portraits, and icon entries for the 10 new researches. Existing `runtime_animation` settings and all prior per-sprite scale, pivot, hitbox, and animation metadata are preserved. The new full-body portraits use the existing 58 × 58 world display size and established pivots/hitboxes; they remain static source art consumed by the v0.7 animation system.

`ArtInventory.json` now records 74 unit sprites, 50 research icons, 42 age-choice aliases, and 238 unique PNGs. The gallery is portable relative to this docs directory and includes the 13 new assets. `NewArtTasks-v0.7.json` records that these assets were generated, mapped, and verified in the packaged data.
