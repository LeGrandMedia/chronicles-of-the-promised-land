# v0.6 first art pass

The game now uses 225 original transparent PNGs in a low-poly toon style. The visual direction draws on Arnold Friberg's monumental silhouettes, broad anatomy, weighty fabric, warm earth colors, and dramatic light. The designs are original illustrations rather than reproductions of his paintings.

| Artwork | Unique images |
|---|---:|
| Units, heroes, animals, and ships | 71 |
| Faction worker variants | 2 |
| Base structures | 17 |
| Faction structure variants | 60 |
| World resources | 5 |
| Research icons | 40 |
| Command icons | 15 |
| Resource and religious-stat badges | 9 |
| Faction emblems | 6 |
| **Total** | **225** |

The 36 age-choice cards reuse their corresponding hero portraits. Unit and building thumbnails reuse their world sprites. The manifest therefore supplies 155 world-sprite entries and 261 icon entries without duplicating the original PNG files.

Open `ArtGallery.html` for searchable previews. The originals are in `../Game/Content/Data/Art/`, with full prompts, source paths, and generation mode recorded in its `production/` folder. All 225 assets were generated through built-in `image_gen`, then copied without altering their image pixels. The audit confirms 1254 by 1254 dimensions, alpha channels, unique image hashes, and matching original-source copies.

Artwork resolves by stable ID and owning faction. The renderer retains aspect ratio and transparency, leaves the painted colors intact, and marks player ownership separately. Left-click targeting ignores transparent sprite corners; right-click targeting keeps more generous bounds for resource orders. Health bars and building workforce labels follow the displayed art bounds.

The game loads compact 512-pixel world textures and 256-pixel UI textures before a local player can become ready in the lobby. The complete set imports 380 cached textures, accounting for 221,511,680 pixel bytes (211.25 MiB). This figure excludes alpha masks and other engine memory. The source images remain unchanged and can be replaced without recompiling the game.

These are static raster sprites and icons. This pass does not include directional animation, rigged 3D meshes, destruction variants, or new terrain tiles. Existing movement effects still animate the sprites' positions. The manifest supports replacing or extending them with animation frames later.

`ArtTasks-v0.6.md` labels every task with its best-suited model. GPT-6 Luna handled inventory, repeated icon briefs, and receipt auditing; GPT-6.1 Sol handled detailed art direction and renderer integration. `Validation-v0.6.md` records the final game checks and the two integration problems found and fixed.
