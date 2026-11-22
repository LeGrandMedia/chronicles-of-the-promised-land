# Dense district readability — v0.9

GPT-6.2 Sol was recommended and used for the fixture and review. The November 22 Win64 Development probe exited 0 with eight captures and zero fixture failures. Game binary SHA-256 `01849280A0BC42FB668D1DEBF8FBB78DA0897872C2FA0926DC4F3CCDB238621C`. [Raw report](Evidence/v0.9/rendering/dense-district-initial/dense-district-tests.json) · [Native log](Evidence/v0.9/rendering/dense-district-initial/district.log) · [Run record](Evidence/v0.9/rendering/dense-district-initial/run.json)

The server accepted and charged for all 13 placements through the ordinary build command. The fixture used clear, pre-explored ground, preloaded age and resources, and completed each accepted foundation for the render. Every placement after the first was within 2.5 cm of a prior building's legal clearance boundary. The client rendered the filtered server snapshot. Building labels come from the catalog and server IDs.

## What the screen showed

Every building had an exact selectable pixel at both zoom limits and camera headings. Across the eight 888 × 500 views, the fixture tested 14,229 integer viewport samples without encountering projected building samples outside the screen or behind a blocking HUD control. The far zoom used every integer pixel in each projected silhouette. The near zoom used a 5 px grid. Exposure below is the share of a building's sampled silhouette whose exact pick returned that building.

| Camera | Laban exact picks and lowest exposure | Gadianton exact picks and lowest exposure |
|---|---|---|
| 0.55×, 0° | 9/9, Crib 43/270 (15.9%). [Capture](Evidence/v0.9/rendering/dense-district-initial/dense-district-laban-far-0.png) | 4/4, Assembly 172/226 (76.1%). [Capture](Evidence/v0.9/rendering/dense-district-initial/dense-district-gadianton-far-0.png) |
| 0.55×, 45° | 9/9, Armory 36/194 (18.6%). [Capture](Evidence/v0.9/rendering/dense-district-initial/dense-district-laban-far-45.png) | 4/4, Assembly 176/214 (82.2%). [Capture](Evidence/v0.9/rendering/dense-district-initial/dense-district-gadianton-far-45.png) |
| 4×, 0° | 9/9, Crib 91/584 (15.6%). [Capture](Evidence/v0.9/rendering/dense-district-initial/dense-district-laban-near-0.png) | 4/4, Assembly 364/476 (76.5%). [Capture](Evidence/v0.9/rendering/dense-district-initial/dense-district-gadianton-near-0.png) |
| 4×, 45° | 9/9, Armory 84/407 (20.6%). [Capture](Evidence/v0.9/rendering/dense-district-initial/dense-district-laban-near-45.png) | 4/4, Assembly 374/453 (82.6%). [Capture](Evidence/v0.9/rendering/dense-district-initial/dense-district-gadianton-near-45.png) |

The Great and Spacious Building obscures much of the Crib at 0° and the Armory at 45°. Its three balconies, the Winery's vats, the Vineyards' trellis grid, and the Market's canopies remain identifiable in the near captures. The Lodge and Secret Place use nearly identical moss roofs and front shelters. Their near views show distinct selectable surfaces but give few visual clues about which is the military building. The evidence comes from geometric sampling and screenshot review. Human recognition and physical touch were outside this fixture.

## Where the art overlaps on the ground

The probe projected the rendered triangles onto a 10 cm ground grid. Its low mask includes geometry at and below 15 cm. A separate read-only slice of the source meshes at a common elevation of 7.5 cm confirmed that the three largest Laban intersections share solid cross-sectional area. The affected shapes are primarily the landmark's bottom foundation and the entry treads at the Quarters.

| Landmark neighbor | Low projected overlap | Solid intersection at 7.5 cm |
|---|---:|---:|
| Laban's Crib | 1.21 m² | 1.194 m² |
| Labanite Quarters | 1.04 m² | 1.081 m² |
| Laban's Armory | 0.32 m² | 0.315 m² |

The Assembly and Lodge share 0.32 m² in the overhead projection and 0 m² in the low mask. They also shared no solid area at the sampled common elevations of 7.5 cm and 12.5 cm. The Crib–Winery and Vineyards–Guard Camp low overlaps are smaller, at 0.05 m² and 0.07 m² respectively.

The measured priorities are the Laban landmark's ground plinth and stair reach, followed by a distinct Lodge roof or porch silhouette. The Laban landmark's height and three-tier profile support its role even when the low geometry is tighter. [Earlier building audit](BuildingReadability-v0.9.md)

## Art refinement and source checks

The Laban landmark uses a 4.60 × 4.65 m chamfered ground plinth, shorter stairs, and a fitted first terrace. The lower masonry and porch feet sit on that terrace. All three balcony roofs and the 5.5988 m height are retained. Geometry below 15 cm reaches 2.7932 m from the origin within the 2.8 m placement radius.

The same-place mesh comparison found 0 m² of intersection with the Crib, Quarters, and Armory in sampled common sections at 7.5 cm, 12.5 cm, 29 cm, 42 cm, and 47.5 cm. The preview showed the narrower plinth and clear separation from the Quarters while retaining the landmark's tiered silhouette. The Lodge preview uses a dark gabled roof above a lower sloped porch, which separates it from the Secret Place's moss canopy at both camera headings. [Source section measurements](Evidence/v0.9/rendering/dense-district-refined/source-section-checks.json)

The full model validator passed all 259 models. The only model files changed were the Laban landmark and the base and faction Lodge models. The model manifest, unit and resource JSONs, and skeletal inputs were byte-identical in the write comparison. [Generator](../Tools/GenerateModels.py)

## Packaged replay

The refined Win64 Development build exited 0 with eight fresh 888 × 500 captures, zero fixture failures, and identical authoritative placements. Its game binary SHA-256 was `012186FA30DC3C72B6C8B90B7A8A0352495B4922AD0EE68BE8ACD76252BBD85E`. The low projected overlaps with the Crib, Quarters, and Armory fell from 1.21, 1.04, and 0.32 m² to 0 m². The landmark and Crib still share 0.26 m² in overhead projection, where geometry at different heights can cover the same ground coordinates. The smaller Crib–Winery and Vineyards–Guard Camp low overlaps still measure 0.05 and 0.07 m². The Assembly–Lodge overhead projection measured 0.36 m², with 0 m² in the low mask. [Refined report](Evidence/v0.9/rendering/dense-district-refined/dense-district-tests.json) · [Native log](Evidence/v0.9/rendering/dense-district-refined/district.log) · [Run record](Evidence/v0.9/rendering/dense-district-refined/run.json)

All buildings remained exactly pickable in each view across 13,900 world samples. At 0°, the Crib kept 15.9% and 15.6% sampled exposure at far and near zoom. At 45°, the Armory rose from 18.6% to 19.6% at far zoom and from 20.6% to 21.4% at near zoom. The near Laban images show the separation between the landmark foundation and the Quarters. [Laban far 0°](Evidence/v0.9/rendering/dense-district-refined/dense-district-laban-far-0.png) · [Laban far 45°](Evidence/v0.9/rendering/dense-district-refined/dense-district-laban-far-45.png) · [Laban near 0°](Evidence/v0.9/rendering/dense-district-refined/dense-district-laban-near-0.png) · [Laban near 45°](Evidence/v0.9/rendering/dense-district-refined/dense-district-laban-near-45.png)

The near Gadianton views show the Lodge's triangular roof clearly beside the camp's rounded canopy. At far zoom, their silhouettes sit within a small cluster. [Gadianton far 0°](Evidence/v0.9/rendering/dense-district-refined/dense-district-gadianton-far-0.png) · [Gadianton far 45°](Evidence/v0.9/rendering/dense-district-refined/dense-district-gadianton-far-45.png) · [Gadianton near 0°](Evidence/v0.9/rendering/dense-district-refined/dense-district-gadianton-near-0.png) · [Gadianton near 45°](Evidence/v0.9/rendering/dense-district-refined/dense-district-gadianton-near-45.png)

An additional **1600 × 900** replay of the refined build exited 0. All eight views gave exact integer-pixel picks for 9/9 Laban buildings and 4/4 Gadianton buildings across 45,140 sampled world pixels. Every sampled building pixel was on-screen and outside blocking HUD controls. These captures verify the refined models at the larger viewport. The matched before-and-after comparison above uses 888 × 500. [Full-size report](Evidence/v0.9/rendering/dense-district-refined-1600x900/dense-district-tests.json) · [Laban near view](Evidence/v0.9/rendering/dense-district-refined-1600x900/dense-district-laban-near-0.png) · [Gadianton near view](Evidence/v0.9/rendering/dense-district-refined-1600x900/dense-district-gadianton-near-45.png)
