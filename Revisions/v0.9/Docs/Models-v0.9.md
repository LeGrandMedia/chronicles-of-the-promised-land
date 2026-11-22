# v0.9 world models

The world mesh library contains 74 units, 17 buildings and five resource nodes, with 163 faction and gate variants. Every asset is original geometry generated from readable Python recipes. The game uses the meshes for world entities and retains the existing PNG library for interface art.

## Art direction

- Nephite buildings use pale cut stone, long blue-green roof ridges and shaded colonnades. Lamanite civic buildings use ochre terraces and red sloped roofs. The Rameumptom rises above its terrace as an open prayer platform.
- Gadianton sites use timber, low hide roofs and rocks or plant cover. Coriantumr buildings have battered cement walls and flat parapets. Shiz buildings use steeper dark roofs, spikes and wine-colored cloth. Laban buildings use pale columns and terracotta hips. Its tiered Great and Spacious Building, low amphora-filled Winery and trellised Vineyards each have a distinct roofline or footprint.
- The unit recipes vary headwear, equipment, silhouette and cloth. The chariot has a driver, two draft horses and independently pivoted spoke wheels. A Curelom has a shaggy mane and horns, a Cumelom has six legs and twin humps, and an elephant has a curved trunk, tusks and a cargo howdah.
- Hagoth’s Vessel carries a bellying sail and deck cargo. The Jaredite Barge has a ribbed shell and a pilot stone. Resource meshes depict a cedar, native ore, a berry shrub, a bound crate and an antlered stag.

The geometry uses broad facets and warm mineral, timber and cloth colors. Monumental silhouettes and earth tones take inspiration from Arnold Friberg’s paintings. The recipes contain no scans, purchased meshes or third-party geometry.

## Contact sheets

The contact sheets render the actual mesh files with a fixed view, facet lighting and ground shadows. Each tile fits its specimen to the frame.

- [Structures](Models-v0.9-structures.png)
- [Characters](Models-v0.9-characters.png)
- [Resources, beasts, chariot and vessels](Models-v0.9-world.png)
- [Unit silhouettes at gameplay scale](Models-v0.9-gameplay-scale.png)

The gameplay-scale sheet uses the game camera’s 30-degree elevation and zoom limits at a 1600 × 900 reference viewport. At the default 12.7 pixels per meter, the timber load, farmer’s hat, trader’s pack and guard’s tall shield read from both camera angles. At the farthest zoom, equipment occupies very few pixels and individual role recognition relies mainly on the largest outlines.

## Generate, validate and export

Run the following from `Revisions/v0.9`. Generation, validation and OBJ export use the Python standard library. Contact sheet rendering also needs Pillow.

```powershell
python Tools/GenerateModels.py
python Tools/GenerateModels.py --validate
python Tools/GenerateModels.py --export 'temple_of_the_land:laban' --out 'GreatAndSpacious.obj'
python Tools/GenerateModels.py --preview
```

`Tools/GenerateModels.py` is the editable source. Building recipes start with `Architecture`, and humanoid configurations are in `PEOPLE`. Regenerate the JSON after changing a recipe. The OBJ and MTL export preserves per-part objects, bone groups and flat material colors for inspection and editing in Blender. The runtime reads the generated JSON. Keep gameplay pivots in the Python recipe when incorporating geometry changes.

## Runtime format

`Game/Content/Data/Models/models.json` maps base definition IDs under `models` and faction or gate keys under `variants`. All paths are relative to the manifest. Variants use `definition:faction`, `definition:gate` and `definition:gate:faction` keys.

Each model uses meters, with X forward, Y right and Z up. Wall segments run along local X. Unit origins are at the feet, while buildings sit on their ground footprint. Ship hulls extend below their decks, and harbor piles extend slightly below ground. `bounds` and `height` enclose the complete rest pose. `footprint` is the centered X/Y extent of its visible geometry.

Part vertices are in model rest coordinates. Triangle indices are zero-based and outward-wound on closed meshes. Each part has a normalized RGBA color and a rigid bone assignment, and accent parts can opt into player tint with `team`. Rig entries store model-space pivots and parent bones. On humanoids, `forearm_l` and `forearm_r` pivot at their elbows and follow `arm_l` and `arm_r`. `shin_l` and `shin_r` pivot at their knees and follow `leg_l` and `leg_r`. Hands, bracers and offhand gear follow the forearms. Calves and feet follow the shins. Beasts have front/rear knee joints, with a middle pair on the six-legged Cumelom. Ships carry hull, sail and oar joints.

The optional `animation` object records a style, the primary equipment and named sockets. Each socket has a bone and a model-space rest position. Archers hold the bow in the left hand, with the `weapon` joint under `forearm_l`. The `draw_hand` socket follows `forearm_r`, and `bowstring_upper`, `bowstring_lower` and `projectile_release` follow the weapon. Slingers supply a projectile release socket at the sling pouch.

Validation covers the catalog and visual manifest mappings, unique names, rig hierarchy, equipment and socket attachments, finite coordinates, triangle indices, nondegenerate faces, bounds and triangle budgets. Structures range from 208 to 1,738 triangles. The two-body Laman and Lemuel uses 2,322 triangles, and the two-horse chariot uses 2,968.

## Live presentation

`RTSScene3D` uses GPU skinning for 147 animated assets and cached procedural geometry for static objects and wall junctions. The original JSON/procedural path remains a fallback. Hierarchical joint transforms and exact triangle picking use only filtered client snapshots. Walking follows travelled distance, chariot wheels keep their phase when stopped, and ships animate their hull, sail and oars separately. Missing resources disappear; units and buildings receive collapse animations on death. See [rendering status](RenderingPlan-v0.9.md) for verified GPU behavior and platform limits.

Attack poses are held from advancing strike timestamps, so the one-step attack flag can be missed by a network snapshot without losing the animation. Bowstrings are two mesh sections of the model that draw toward the animated hand socket. `RTSProjectiles3D.cpp` renders arrows or stones from release sockets to an already visible target's sampled position. These bounded visual flights do not alter damage, collision or authority. They disappear in unexplored/unseen fog and are omitted in reduced-motion mode.

`-RTSV09SceneTest` runs actual UE geometry/projection/picking/pose fixtures; it deliberately freezes gameplay to isolate presentation. Evidence lives under `Docs/Evidence/v0.9/rendering`. The complete-library run loaded all 259 mapped models, with projection error below 0.001 pixels across six camera/faction fixtures. Gameplay interaction, rendered battle performance and mobile-device validation are separate checks.
