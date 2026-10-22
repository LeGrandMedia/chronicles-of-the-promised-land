# Structures

Current base costs, ages, construction statistics, and faction availability: [[Data and Assets]]. Shared construction, placement, population, and repair rules belong to system notes. Research details are in [[Upgrade Catalog]]. These sections retain each structure's distinct purpose and acquisition or interaction exceptions.

## Encampment of War

Military production building, available from Age II. The Gadianton display name is Secret Place (stable ID: `encampment_of_war`). The House of Laban display name is Laban's Guard Camp. Faction rosters are defined by the catalog.

## Fields of Grain

Renewable food building with ten worker slots; current footprint is catalog data. Laborers assigned here become [[Units#Tiller of the Earth|Tiller of the Earth]]. The House of Laban calls it Vineyards (stable ID: `fields_of_grain`). Laban buildings use distinct silhouettes that remain readable at far zoom.

## Gathering of Flocks

Herding building that trains separate Jaredite livestock types and supports passive sheep food production. Fields and flocks have ten worker slots; each flock holds up to ten living or queued beasts. Destroying a flock scatters its sheep and converts them to the attacker. Gadiantons cannot build their own farms/flocks, but their workers may gather Food from other players' completed farms/flocks, including allies. Jaredite flock-building limit is five, raised to seven by the Age IV Enlarge the Flocks upgrade. See [[Data and Assets]].

## Hill of Ore

Mining building for a finite Precious Things deposit; it must overlap an active deposit. Mining depletes the deposit and its linked hill. Laborers assigned here become [[Units#Worker in Ore|Worker in Ore]].

## House of Curious Workmanship

Equipment and builder research building. For Jaredites it supports the livestock and cement-building progression; exact upgrades are in [[Upgrade Catalog]]. House of Laban calls it Laban's Armory (stable ID: `house_of_curious_workmanship`).

## Judgment Seat

Retired by the v0.4 playtest direction. It was an anti-infiltration and political-agent building; later Gadianton infiltration and sabotage moved to Lasciviousness, Lawyers, towers, and Storehouse mechanics. Retained here to preserve the original identity and backlink target.

## Place of Many Waters

Shoreline dock for water transports; placement touches navigable water and shore. Glowing Stones improves Jaredite Barge vision to 14m. House of Laban calls it Laban's Harbor (stable ID: `place_of_many_waters`).

## Place of Trade

Market for the player-set routes used by [[Units#Trader in Precious Things|Trader in Precious Things]]; traders generate Precious Things when passing near own workers rather than trading between two markets. House of Laban calls it Laban's Market (stable ID: `place_of_trade`).

## Rameumptom

Lamanite religious tower. It starts as a religious building and, after By the Pride of Their Hearts, converts nearby enemies into Zoramite Captains. Order of the Nehors lets each Priestcrafter generate +0.25 Precious Things/s as well as Anger. Age III upgrades can grant +5 Anger on each enemy-caused unit or structure loss and triple conversion range and line of sight. The stable building ID is `temple_of_the_land`; all conversion channels take five seconds with no cooldown, and converted units are immune to further conversion. See [[Data and Assets]] and [[Conversion and Ammonites]].

## Seat of the People

Headquarters for workers, age advancement, and hero choices; laborers can right-click it to drop off resources. The Seat of the People is the largest standard building; the Great and Spacious Building is larger. Laban homes have a smaller footprint. The game has four ages; each age-up is a distinct hero choice that grants its faction perk, and the selected hero can be retrained only after death. Jaredites recruit Cureloms here, with a separate ten-animal bootstrap cap. Cumeloms are Age-III flock recruits; the earlier HQ/husbandry route is superseded. House of Laban calls the building Laban's Crib; other faction display names are in [[Data and Assets]].

## Storehouse of Provisions

Resource drop-off building and economic-upgrade site; laborers can right-click it to drop off resources. Gadianton players can set an enemy-source and home Storehouse raid route here; newly trained Bands of Kishkumen inherit it. Notify the target player when robbed. See [[Gadianton Plunder and Stealth]] and [[Upgrade Catalog]].

## Temple of the Land

Religious building that generates its civilization's religious resource and supports religious units/conversion. The Lamanite Temple is called the Rameumptom (stable ID: `temple_of_the_land`). House of Laban calls it the Great and Spacious Building: it unlocks in Age IV, is limited to one, generates five times Winery Drunkenness (2.5/s base, 3/s with Fermented Reserves), and weakens enemies within 8m. Drunken Concubines train at the Winery. Their 5m aura reduces enemy movement 20%, attack speed 25%, and maximum HP 15%, preserving current HP fraction; nearby laborers gain +20% gathering and construction speed. Each affected area is shown on the map. The Gadianton religious role is represented by one **Secret Combination Lodge** (`secret_combination_lodge`): trains Lawyers and generates 0.16 Lasciviousness/s until scouted. They have no additional `temple_of_the_land` in their whitelist. The v0.4 request uses both “Temple of the Lord” and “Secret Combination Lodge”; treat these as the same intended role, not a second buildable temple. Nephite religious units include the Teacher of the Word. See [[Conversion and Ammonites]] and [[Data and Assets]].

## Winery

House of Laban's primary religious building; trains Drunken Concubines and generates 0.5 Drunkenness/s with no Food. Age-III Winery Abundance adds 3 Food/s. Age-III Fermented Reserves increases Winery Drunkenness by 20%; the Great and Spacious Building generates five times the Winery rate. Stable ID: `winery`; see [[Data and Assets]].

## Tents and Habitations

Population housing; current housing and player caps are in [[Data and Assets]]. House of Laban calls it Labanite Quarters (stable ID: `tents_and_habitations`).

## Tower of Watchmen

Defensive tower with ranged attack and stealth detection. Lamanites do not have this structure; opponents use towers to reveal stealth. House of Laban calls it Laban's Watchtower (stable ID: `tower_of_watchmen`).

## Works of Defense

Builds wall segments between player-chosen endpoints; queued endpoint pairs can form continuous walls. Existing segments can be converted into gates with the Age II Gate Segment upgrade, available to all wall-building factions including House of Laban. Jaredite animals can build multiple wall segments at once. Nephites and Jaredites unlock wall construction through faction upgrades; House of Laban can build walls directly. See [[Jaredite Cement Construction]] and [[Upgrade Catalog]].
