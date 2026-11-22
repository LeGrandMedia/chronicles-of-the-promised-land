# v0.9 building readability audit

Recommended and actual task model is GPT-6.2 Sol. This read-only audit compares the generated building models with the [canonical structure notes](../../../book%20of%20mormon%20RTS/04%20Structures/Structures.md) and the [settled GPU fixture](Evidence/v0.9/rendering/gpu-settled/scene3d-tests.json). The fixture renders at zoom 1.65 and alternates camera headings between 0° and 45°. The far-zoom sizes below project the same source vertices at zoom 0.55 into a 1600 × 900 viewport. Areas are the actual model-space XY bounding rectangles in square meters. Placement continues to use the catalog radii.

Follow-through, November 22: seven generated building files now include the recipe refinements below and passed the [60-check building scene](Evidence/v0.9/rendering/building-readability/scene3d-tests.json). The proposed shadow fix is implemented and measured in [Performance-v0.9](Performance-v0.9.md). Queued corner seams and unused tips are fixed with [102-check mitered junction evidence](WallJunctionPlan-v0.9.md). The remaining district-overlap, gate-silhouette and physical-mobile observations below are still open; the original audit measurements remain useful baseline evidence.

## Civic scale across the six factions

These measurements describe the models in the settled captures before the recipe refinements below.

| Faction | Seat XY area and height | Religious building XY area and height | Home XY area | What reads in the current scene |
|---|---:|---:|---:|---|
| Nephites | 22.66 m², 3.50 m | 22.92 m², 2.83 m | 5.76 m² | The two blue-green roofs and porticos nearly tie at far zoom. Projected envelopes are 42.7 × 34.2 px for the Seat and 42.4 × 33.6 px for the Temple. [Capture](Evidence/v0.9/rendering/gpu-settled/scene3d-gpu-nephites.png) |
| Lamanites | 23.64 m², 2.52 m | 20.23 m², 4.13 m | 6.00 m² | The Council has the broader terrace. The Rameumptom reads as a narrow elevated prayer platform and intentionally occupies more vertical screen space. [Capture](Evidence/v0.9/rendering/gpu-settled/scene3d-gpu-lamanites.png) |
| Gadianton | 16.23 m², 2.17 m | 15.17 m², 2.38 m | 5.76 m² | Secret Assembly is barely wider than the 15.74 m² camp. The camp and Lodge share the same low shelter roof recipe, and both are taller than the Assembly. The fixture poses a generic Temple for this faction. Its buildable religious building is the Secret Combination Lodge. [Capture](Evidence/v0.9/rendering/gpu-settled/scene3d-gpu-gadianton.png) |
| Coriantumr | 25.60 m², 4.26 m | 23.72 m², 3.69 m | 6.03 m² | The Hall's stacked parapets and the Temple's receding steps are distinct. Their frontal far-zoom widths differ by 0.4 px, so the Hall's height carries the scale hierarchy. [Capture](Evidence/v0.9/rendering/gpu-settled/scene3d-gpu-coriantumr.png) |
| Shiz | 21.11 m², 3.92 m | 23.72 m², 3.73 m | 6.03 m² | The Temple is 12% broader by footprint and projects larger than the Hall at far zoom, 43.3 × 37.5 px compared with 40.4 × 35.4 px. [Capture](Evidence/v0.9/rendering/gpu-settled/scene3d-gpu-shiz.png) |
| House of Laban | 19.93 m², 3.15 m | 36.71 m², 5.60 m | 6.38 m² | Three full balconies and a broad crown make the Great and Spacious Building dominant. Laban's Crib has a lower portico and reads wider than the quarters. [Capture](Evidence/v0.9/rendering/gpu-settled/scene3d-gpu-laban.png) |

Housing stays within 5.76–6.38 m² across all six factions. Laban's quarters occupy about 17 × 18 px at the far frontal view, compared with 29 × 33 px for Laban's Crib. The existing housing dimensions meet the small-home requirement. Open fields and flock enclosures are roughly 44 m² but mostly 1–2 m tall. Include them as open plots in a side-by-side render. Narrow watch and cement towers range up to about 5 m tall and occupy only about 4 m² on the ground.

The recipe refinements below keep the Nephite and Shiz temple elevations and change only their horizontal scale. The Lamanite and Coriantumr Seats already lead within their rosters. A future Gadianton gallery should place the actual Lodge and camp alongside the adjusted Assembly before changing their shared `lodge` roof recipe.

## Refined recipes

| Building | Geometry change | Actual XY area | Far-zoom envelope at its fixture heading |
|---|---|---:|---:|
| Nephite Temple | Horizontal scale 0.92 | 22.92 → 19.40 m² | 42.4 × 33.6 → 39.0 × 32.0 px |
| Shiz Temple | Horizontal scale 0.88 | 23.72 → 18.37 m² | 43.3 × 37.5 → 38.1 × 35.6 px |
| Gadianton Assembly | Horizontal scale 1.08 | 16.23 → 18.93 m² | 36.6 × 26.4 → 39.6 × 27.8 px |
| Laban Winery | Lower main hip, a lean-to on one side, two exposed vats on the other | 15.86 m² | 26.9 × 26.6 → 26.9 × 23.7 px |
| Laban Armory | Raise the rear furnace stack and smoke collar by 0.42 m | 15.71 m² | 26.6 × 26.1 → 26.6 × 28.6 px |

The Nephite Seat's area leads the Temple by 17%, the Shiz Hall's by 15%, and the Assembly's leads its camp by 20%. The Gadianton Assembly's furthest vertex is 2.985 m from its origin within its 3 m placement radius. The Winery vats each measure 0.84 × 0.86 m. The Winery, Guard Camp and Armory now project about 24, 26 and 29 px high at Laban's far frontal view. These are projected source measurements. Gameplay camera captures will verify how the two cues read beside the other buildings.

The generated-model validator checks that the Seat leads adjacent compact structures by area, housing keeps a smaller footprint, the Great and Spacious Building leads the Crib, the Gadianton Assembly fits the catalog radius, and the Laban cues retain roof clearance and vat size. The in-memory run passed and reproduced all 151 unit and resource JSON files byte-for-byte, including the 147 inputs to the skeletal bake. Seven building JSONs are affected, including base and faction aliases.

## Placement radius and overhang

The catalog radius is a circle about the entity origin, and `Build` passes it to `Walkable`. `Walkable` checks the requested radius plus 88% of each existing building's radius. Foundation and roof values below come from the generated first `dressed_foundation` mesh and the combined roof assembly, including its fascia and parapet. Each cell gives its horizontal X × Y dimensions and the farthest actual XY vertex from the placement origin. The bounding rectangle corners are not used for the circular measurement. Values are shown in meters.

| Faction | Building | Placement radius | Foundation X × Y, max radius | Roof X × Y, max radius |
|---|---|---:|---:|---:|
| Nephites | Seat | 3.00 | 4.70 × 4.49, 3.13 | 4.44 × 4.35, 3.13 |
| Nephites | Temple | 2.80 | 4.62 × 4.54, 3.16 | 3.93 × 3.48, 2.84 |
| Nephites | Home | 1.60 | 2.38 × 2.42, 1.68 | 1.94 × 2.26, 1.64 |
| Lamanites | Council | 3.00 | 4.22 × 4.54, 3.07 | 3.35 × 2.89, 2.86 |
| Lamanites | Rameumptom | 2.80 | 4.43 × 4.06, 3.02 | Open platform |
| Lamanites | Home | 1.60 | 2.38 × 2.42, 1.68 | 2.23 × 2.23, 1.82 |
| Gadianton | Assembly | 3.00 | 3.90 × 3.90, 2.68 | 4.04 × 3.85, 2.76 |
| Gadianton | Lodge | 2.00 | 3.44 × 3.58, 2.40 | 3.83 × 3.56, 2.64 |
| Gadianton | Home | 1.60 | 2.38 × 2.42, 1.68 | 1.94 × 2.26, 1.64 |
| Coriantumr | Hall | 3.00 | 4.95 × 4.76, 3.27 | 4.67 × 4.48, 3.37 |
| Coriantumr | Temple | 2.80 | 4.66 × 4.70, 3.14 | 1.85 × 2.53, 1.93 |
| Coriantumr | Home | 1.60 | 2.47 × 2.38, 1.67 | 2.03 × 2.09, 1.65 |
| Shiz | Hall | 3.00 | 4.55 × 4.16, 2.95 | 4.11 × 4.24, 2.86 |
| Shiz | Temple | 2.80 | 4.66 × 4.70, 3.14 | 1.85 × 2.53, 1.93 |
| Shiz | Home | 1.60 | 2.47 × 2.38, 1.67 | 2.03 × 2.09, 1.65 |
| Laban | Crib | 3.00 | 4.24 × 4.09, 2.84 | 4.28 × 4.05, 2.99 |
| Laban | Great and Spacious Building | 2.80 | 5.92 × 5.82, 3.92 | 4.13 × 4.95, 3.19 |
| Laban | Quarters | 1.60 | 2.47 × 2.38, 1.67 | 2.68 × 2.09, 1.65 |
| Laban | Winery | 2.20 | 3.88 × 3.84, 2.56 | 3.95 × 3.09, 2.73 |

The Great and Spacious Building's foundation extends 1.12 m past its catalog radius along the most exposed diagonal. The Lodge and Winery roofs also overhang their gameplay circles. These measurements flag where to inspect tightly spaced construction and unit paths. The gallery has generous spacing and does not validate overlap in a legal district. Sources are [the catalog](../Game/Content/Data/catalog.json), [the model manifest](../Game/Content/Data/Models/models.json) and [the placement check](../Game/Source/Chronicles/RTSNavigation.cpp).

## Original Laban district observations

The current captures show the Crib, quarters, Storehouse, Winery and landmark. The Great and Spacious Building projects 41 × 48 px and keeps its three-tier outline. The Winery, Guard Camp and Armory each project about 27 × 26 px. Their roof contours and open areas have to identify them at that size. The current Winery reads as a small hip roof with a second hip roof beside it. It can resemble a compact Crib or Guard Camp once amphora details shrink to a few pixels. The Vineyards' wide trellis grid, Market's two canopies, Harbor's long wharf and Watchtower's tall narrow shaft already provide different large forms in the source geometry. The settled fixture omits those structures.

Capture one actual Laban district with Crib, quarters, Winery, Vineyards, Great and Spacious Building, Storehouse, Armory, Market and Guard Camp at both zoom limits and both headings. Compare the lean-to and vats, offset chimney and open training yard before reading selection labels. The original settled screenshots cover the Crib, quarters, Storehouse, Winery and landmark.

## Wall alignment

The generator runs the long axis of all regular and cement walls and corresponding gates along local X. The server gives new segments the drag angle, and gate conversion reuses the entity's heading. Stone Works of Defense is 4.15 m long with 4.15 m center spacing. Cement walls are 2.154 m long against 2.15 m spacing. Both gates have slightly wider towers that overlap their neighboring segments. Settled captures show straight runs meeting the gate. Palisade gates are visibly heavier than their timber runs. A small recipe change for the Lamanite and Gadianton gates would use timber post and lintel colors with a tapered wood profile while preserving the opening and axis.

The original gallery's L-shaped corner used directly spawned segments 4 m apart. Subsequent fixtures use real queued wall orders and filtered snapshots: they reproduced and closed the predicted seams at 45° and 90°, then verified gate conversion, picking and remembered geometry. See [wall placement, escape and junction results](WallJunctionPlan-v0.9.md). Gate navigation still admits a wider path than its visible aperture; art and clearance need a coordinated follow-up.

## Original shadow diagnosis

The six settled captures show little visible contact shadow under the units and buildings. In `RTSScene3D.cpp`, the orthographic camera places the ground center 30,000 cm down its view direction. `WarmSun` is the only directional light that casts shadows and caps movable cascaded shadow coverage at 25,000 cm. UE5.3's `FDirectionalLightSceneProxy::GetCSMMaxDistance` uses that value scaled by `r.Shadow.DistanceScale`. The fixture log records a distance scale of 1.0, shadow quality 5, a cascade limit of 10 and virtual shadows disabled. Terrain disables casting and can still receive shadows. [Fixture renderer log](Evidence/v0.9/rendering/gpu-settled/scene3d-tests.log)

At the fixture's default zoom, a 1600 × 900 view covers approximately 25,546–32,970 cm of ground depth; at zoom 0.55 it reaches approximately 38,900 cm. The implemented 45,000 cm sun-shadow distance now covers this view without changing camera projection. Reviewed captures and the paired 200/800-unit presentation measurements are recorded in [Performance-v0.9](Performance-v0.9.md).
