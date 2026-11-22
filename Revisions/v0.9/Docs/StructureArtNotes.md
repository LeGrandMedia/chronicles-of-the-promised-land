# Structure and resource art — v0.6

Completed this art pass: 17 base building sprites, 44 named faction building variants, and 5 resource sprites (66 distinct assets). The remaining 16 Coriantumr/Shiz utility-building variants were transferred to the root art pass; their briefs are in Game/Content/Data/Art/production/structure-transfer-briefs.json. The planned combined set is 77 buildings and 5 resources (82 assets).

All files stay within Revisions/v0.6. No catalog, manifest, C++, or older revision was changed in this task.

## Locations and provenance

- Base structures: Game/Content/Data/Art/buildings/<id>.png
- Faction variants: Game/Content/Data/Art/buildings/<id>@<faction>.png
- Resources: Game/Content/Data/Art/resources/{tree,ore,berries,crate,wild_game}.png
- Each asset has a matching Game/Content/Data/Art/production/<asset-id>.json receipt with the complete prompt, original generated PNG source, and destination.

Each sprite came from one built-in image-generation request. Images were copied from the original tool output without resizing, cropping, padding, background removal, or alpha modification. Original source images remain in the generated-images directory named in each receipt.

## Style and silhouettes

Original ancient structures use chunky low-poly/toon forms, cream sandstone, cedar, bronze, and muted teal, ochre, crimson accents. Monumental massing, warm upper-left light, and cool shadows take inspiration from Arnold Friberg. Orthographic isometric framing looks down roughly 30 degrees; entrances generally face bottom-right. Transparent cutouts have no text or terrain pedestal. Integral farm soil, flock courtyards, mine rock, and harbor decking communicate the building function.

Named variants receive distinct architectural forms: Lamanite Rameumptom is an open elevated prayer platform; Laban's Great and Spacious Building is a lavish multilevel palace; Laban's Crib is a patrician manor. Faction homes, headquarters, temples, farms, mines, workshops, trading places, warehouses, camps, towers, defenses, and harbors use functional silhouette differences rather than color substitutions.

Resources are individually readable: one faceted tree crown and trunk, a rock outcrop with bronze/gold veins, red berries, one bound wooden crate, and a brown stag.

## Checks and integration guidance

All 66 assets completed here were visually reviewed from the tool output. A read-only PNG audit confirms 1254×1254 source dimensions, transparent and opaque alpha values, intact source files, and matching prompt receipts. No visible pixels touch the canvas boundary after the headquarters replacement. The previous base seat_of_the_people clipped its right corner and was regenerated; its replacement has an 83-pixel minimum transparent margin.

The base works_of_defense has a narrow 5-pixel right margin but a visibly complete silhouette. gathering_of_flocks@coriantumr and wild_game have 9-pixel minimum margins; their silhouettes are complete. Prompts requested larger transparent safety margins, but generated occupancy varies. Avoid assuming a uniform alpha footprint when sizing.

Suggested initial renderer settings: buildings at approximately 132 screen pixels using pivot (0.5,0.77), tall towers near (0.5,0.88), and low-wide farms/walls near (0.5,0.68). Resources around 60 pixels use a ground-contact pivot; tree/stag should be anchored near the trunk/hooves. Preserve aspect ratio and native alpha. These are starting values for game preview, not baked image edits. The root preview already confirmed the base building and tree style reads well at these sizes.

At this handoff, the shared directory contained 73 buildings and 5 resources; the root's last four transferred variants were still in production. All 66 assets owned by this pass are present, with no missing designs or generation failures.

