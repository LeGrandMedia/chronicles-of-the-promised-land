# Data and Assets

Implementation root: `Revisions/v0.8`. Authority: [[Design Contract]]. The 21 current requests and eight recovered gaps are implemented; the packaged v0.8 build passed native, rendered-input, and four-process localhost validation. See [[Validation Report]]. These are implementation references, not permission to replace unmet intent with code behavior.

| Need | Reference |
|---|---|
| IDs, base data, faction whitelists, age choices, research | [catalog.json](../../Revisions/v0.8/Game/Content/Data/catalog.json); fields consumed by [RTSCatalog.cpp](../../Revisions/v0.8/Game/Source/Chronicles/RTSCatalog.cpp) |
| Display labels/aliases/descriptions | [names.json](../../Revisions/v0.8/Game/Content/Data/names.json) |
| Sprites/icons, anchors, picking/animation metadata | [visuals.json](../../Revisions/v0.8/Game/Content/Data/visuals.json) |
| Formulas, abilities, commands/collision | [C++ source](../../Revisions/v0.8/Game/Source/Chronicles); not every mechanic is in the catalog |
| Latest requested revisions | [User notes](../../Revisions/v0.8/Docs/UserNotes-v0.8.txt) · [checklist](../../Revisions/v0.8/Docs/RevisionTasks-v0.8.md) |
| Asset replacement/preview | [ArtReplacement](../../Revisions/v0.8/Docs/ArtReplacement.md) · [gallery](../../Revisions/v0.8/Docs/ArtGallery.html) |

The `balance_v04` object is legacy metadata, not a complete balance source. Use the v0.8 catalog for definitions and verify runtime formulas in v0.8 source. Preserve concise copy and meaningful design-only exceptions; do not duplicate prices/stat tables in faction/entity summaries.

Keep stable IDs when renaming. Display “Precious Things”; legacy `wealth` keys and `gather_wealth` voice events remain compatible identifiers. Jobs reuse worker identity; aliases create no extra buildings. Content needs reachable acquisition/producer/builder routes and faction filtering.

## Assets

Original faceted low-poly toon art: monumental readable silhouettes, warm dramatic light, distinct faction equipment and team cues. Keep text separate. Gameplay collision/range is independent of art scale/hit bounds.

The v0.7 asset inventory records 238 unique PNGs; see the v0.8 manifest before adding or replacing assets. Units have state-based motion and death presentation; buildings have destruction presentation. These 2D effects are not a rigged-3D library. v0.8 presentation behavior passed the reported native and rendered-input checks; see [[Validation Report]].

Retain source, prompt/license, charge and runtime mapping per asset. Verify alpha, silhouettes, anchors, selection, action motion and game-scale performance. Replacing art must preserve gameplay. Cover every active unit, structure, resource, command and research icon. [[Voice Lines]] is the single dialogue source; recordings remain separate production work. Track actual costs against the $200 asset/generation ceiling; missing receipts do not establish zero spend.
