# Battlefields in v0.9

2026-11-22. Recommended model: GPT-6.2 Sol for gameplay/UI and GPT-6 Luna for copy. Implementation/integration: Codex parent agent; selection and balance: GPT-6.2 Sol; description cleanup: GPT-6 Luna.

The lobby now offers 15 battlefields, with terrain previews, three cards per page, and a persistent selected-map name. The host chooses the map; changing it clears human readiness. Smaller windows put the secondary description in the hover tooltip to keep names readable. Maps use the same authoritative integer in LAN snapshots. The original indices 0–2 remain stable.

| Map | Layout |
|---|---|
| Bountiful | Green coastal plains with two sheltered bays |
| Moronihah | Broken mountain ring with four gaps |
| Ammonihah | Four stone quarters around open avenues |
| The Land Northward | Dry, sparse woodland and paired ridges |
| The Land Southward | Winding river with three crossings |
| The Narrow Neck of Land | Two seas and a central land passage |
| Middoni | Rocky enclosures with central and outer approaches |
| The Hill Cumorah | Central rocky hill and four surrounding approaches |
| The Waters of Mormon | Sheltered lake with wooded shores |
| Waters of Ripliancum | Large lake with flanking land routes and space for ships |
| The Valley of Lemuel | Long valley between interrupted ridges |
| The Great and Spacious Courtyard | Broad avenues and four stone terraces |

Zarahemla Valley, Waters of Sidon, and Land of Many Waters remain available. These are invented competitive layouts inspired by the names and broad landscape motifs, not reconstructions of real locations. The narrow-neck motif follows [Alma 22:32](https://www.churchofjesuschrist.org/study/scriptures/bofm/alma/22?lang=eng); names and relative landscape inspiration also use the Church's [illustrative Book of Mormon map](https://www.churchofjesuschrist.org/bc/content/shared/content/english/pdf/language-materials/09410_eng.pdf).

## Implementation

`RTSMaps.cpp` is the single registry for stable IDs, display names, short descriptions, terrain recipes, palettes and grove spacing. Terrain, minimap and lobby previews read the same definitions. Gameplay still uses the 160 m square and four established starting positions. New terrain and whole groves mirror across both axes. Each opening retains nine nearby trees, two berry bushes, two wild animals and an ore deposit; extra expansion resources follow the terrain.

Shoreline placement and boat clearance now work on every water map. New layouts check the mover's radius against terrain edges. Ships can pass through shallow fords on the two river maps. Ordinary land units retain traversable routes across all maps, so the current land-focused bots can play them. The bots do not yet plan transport invasions or naval strategies.

The new hill and ridge meshes use continuous faceted surfaces; Ammonihah and the Courtyard use low stone terraces. The click picker checks the submitted terrain surface so units hidden by a ridge cannot be selected through it. These rocky areas block movement; this update does not add a climbable elevation or high-ground combat system.

The original three maps preserve their existing terrain and land-clearance rules. A subsequent [opening fairness pass](OpeningFairness-v0.9.md) rotated the starting formation toward local resources on all 15 maps and excluded extra grove trees within 22 m of each Seat. Historical balance probes can explicitly restore the old formation and grove placement. Terrain colors share the central palette implementation.

## Verification

Current packaged run: [map tests](Evidence/v0.9/maps/opening-raids-final/maps-1600x900/v09-maps.json), executable SHA-256 `CC8D75CDE64C6AF89877698515480D7853C672A63E667A015C41BDBF051042B1`. Windows installer and Android identities are recorded in the [artifact ledger](Evidence/v0.9/platform/opening-raids-final-artifacts.json).

All 4,234 checks passed across 15 maps. The fixture starts actual matches, checks distinct and mirrored terrain, flood-fills actual obstacles at elephant clearance, verifies routes between all four starts and access to all resource objects, checks equal initial laborer distances and clear berry approaches, checks opening supplies, and builds a real dock and trains a ship on every water map. These checks do not establish human competitive balance or physical-device LAN compatibility.

The earlier B1 package's capture probe passed at [1600×900](Evidence/v0.9/maps/maps-balanced/captures-1600x900/v09-map-captures.json) and [888×500](Evidence/v0.9/maps/maps-balanced/captures-888x500/v09-map-captures.json): 15 battlefield images and five lobby pages at each size. Every map button and both paging arrows were exercised, including authoritative map choice and clearing readiness. The parent agent reviewed the hill, narrow-neck and lobby captures. These images predate the opening formation adjustment and are not a human playtest. The current package passed its [Narrow Neck host/client test](Evidence/v0.9/network/opening-raids-final-narrow-neck/run.json) over same-computer loopback.

Reproduce with `Tools/Test-MapsRevision-v09.ps1 -EvidenceName <new-name> -Suites maps,captures`. Balance comparisons accept `-RTSBalanceMap=<index>`; map-dependent evidence belongs in [CivilizationBalance-v0.9.md](CivilizationBalance-v0.9.md).

