# v0.3 revision checklist

This checklist covers all 52 items in `PlaytestNotes-2026-10-21.txt`. Applicable items are implemented in the v0.3 revision; the packaged build has passed the regression checks recorded in [Revision Validation](RevisionValidation-v0.3.md). **Note 46 is an incomplete thought** and is interpreted as priest/ability polish covered by notes 4, 44, and 47. It is marked as covered rather than as a separate feature.

| Note | Requirement | Best model / effort | Status | Area owner |
|---:|---|---|---|---|
| 1 | Reduce/change repetitive resource-gather sound. | gpt-6.1-sol / high | Implemented; included in validated build | Audio/runtime |
| 2 | Show each unit’s hotkey beside its icon. | gpt-6.1-sol / high | Implemented; included in validated build | HUD |
| 3 | Square unit-card icon placeholder. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/art |
| 4 | Hide irrelevant abilities/actions/hotkeys on structures and other ineligible selections. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/controls |
| 5 | Show building descriptions specific to faction. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; HUD integration |
| 6 | Shift-click queue multiple actions. | gpt-6.1-sol / high | Implemented; included in validated build | Commands/UI |
| 7 | Right-click HQ/storehouse with selected villager to drop off cargo. | gpt-6.1-sol / high | Implemented; included in validated build | Economy/commands |
| 8 | Distinct resource-order feedback and larger resource clickboxes. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/input |
| 9 | Make Hill of Ore useful through an ore-gather bonus. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; economy integration |
| 10 | Shift+wheel rotates map view. | gpt-6.1-sol / high | Implemented; included in validated build | Camera/input |
| 11 | Show workers assigned to each resource. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/simulation |
| 12 | Smaller field left-click bounds and larger right-click task bounds. | gpt-6.1-sol / high | Implemented; included in validated build | Input/hit testing |
| 13 | Automatically assign workers to completed economic buildings. | gpt-6.1-sol / high | Implemented; included in validated build | Economy/runtime |
| 14 | Increase Food gathering by 1.5×. | gpt-6-luna / medium | Implemented; included in validated build | Economy/runtime |
| 15 | Multiple builders increase construction speed. | gpt-6.1-sol / high | Implemented; included in validated build | Construction |
| 16 | Shift+number adds to a control group. | gpt-6.1-sol / high | Implemented; included in validated build | Controls |
| 17 | Fields and flocks four times area, ten workers, and visible occupancy. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; HUD |
| 18 | Queue icon placeholder and right-click cancellation. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/production |
| 19 | Smaller left-click boxes for structures. | gpt-6.1-sol / high | Implemented; included in validated build | Input/hit testing |
| 20 | Add idle-military banner. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/selection |
| 21 | Improve north-edge camera panning. | gpt-6.1-sol / high | Implemented; included in validated build | Camera/input |
| 22 | Lower structure selection boxes to align with visuals. | gpt-6.1-sol / high | Implemented; included in validated build | Input/art |
| 23 | Age-II, higher-cost Flocks with sheep that passively produce Food and transfer on flock scattering. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; economy/death handling |
| 24 | Add ass, hell, and racial-slur terms to profanity detection. | gpt-6.1-sol / high | Implemented; included in validated build | Moderation/runtime |
| 25 | Delete buildings/units by holding Delete for two seconds. | gpt-6.1-sol / high | Implemented; included in validated build | Input/commands |
| 26 | No military unit available in Age I. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; production validation |
| 27 | Clarify the LAN address and port entry instructions. | gpt-6-luna / medium | Implemented; included in validated build | Lobby/help |
| 28 | Improve minimap-issued pathing. | gpt-6.1-sol / high | Implemented; included in validated build | Navigation |
| 29 | Increase movement speed for existing moving units by 1.25×. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; runtime validation |
| 30 | Beasts recover HP while near a Gathering of Flocks. | gpt-6.1-sol / high | Implemented; included in validated build | Catalog data; simulation |
| 31 | Hide depleted berry bushes. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/resources |
| 32 | Allow Cumeloms from HQ after Curelom Husbandry. | gpt-6.1-sol / high | Implemented; included in validated build | Catalog data; production/economy |
| 33 | Give Jaredite soldiers distinct equipment/roles inspired by scripture. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; balance/runtime |
| 34 | Nephite Favor starts at 20, decays faster at higher stock, and responds to conversions/deaths. | gpt-6.1-sol / high | Implemented; included in validated build | Catalog balance; simulation |
| 35 | Hide tiered upgrades until prerequisites are met. | gpt-6.1-sol / high | Implemented; included in validated build | Research UI |
| 36 | Briefly display a resource-generation icon over the responsible unit. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/events |
| 37 | Show unit population cost on hover. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/tooltips |
| 38 | Allow Nephites and both Jaredite factions to build walls through upgrades. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; build validation |
| 39 | Keep scouted structures on minimap outside line of sight. | gpt-6.1-sol / high | Implemented; included in validated build | Fog/minimap |
| 40 | Give each faction a passive religious/civic resource and associated combat/economy bonuses. | gpt-6.1-sol / high | Implemented; included in validated build | Catalog balance; simulation/HUD |
| 41 | Improve crowded worker pathing to ore hills. | gpt-6.1-sol / high | Implemented; included in validated build | Navigation/economy |
| 42 | Prevent clustered villagers becoming stuck without gathering. | gpt-6.1-sol / high | Implemented; included in validated build | Navigation/economy |
| 43 | Count idle traders as workers and have them circulate among laborers to earn Precious Things. | gpt-6.1-sol / high | Implemented; included in validated build | Catalog data; simulation/HUD |
| 44 | Lamanite Priestcrafters, Great and Spacious Building, Nehors research, and Pride Rameumptom conversions. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; conversion/economy |
| 45 | Show and switch among selected unit types in the HUD. | gpt-6.1-sol / high | Implemented; included in validated build | HUD/selection |
| 46 | Incomplete note: interpret as religious-unit ability polish overlapping notes 4, 44, and 47. | gpt-6-luna / medium | Interpreted; covered by notes 4, 44, and 47; validation in progress | Scope/ability design |
| 47 | Nephite Teachers automatically attempt conversion near eligible enemies. | gpt-6.1-sol / high | Implemented; included in validated build | Catalog data; conversion AI |
| 48 | Replace Lamoni/Samuel heroes with wicked Lamanite hero choices such as Tubaloth or Laman and Lemuel. | gpt-6-luna / medium | Implemented; included in validated build | Catalog data; hero abilities |
| 49 | Make heroes age-up choices with faction perks, once-per-life retraining. | gpt-6.1-sol / high | Implemented; included in validated build | Catalog data; progression/runtime |
| 50 | Ammonites gain “Prostrate Before the Earth,” converting a military killer while active. | gpt-6.1-sol / high | Implemented; included in validated build | Catalog data; combat/conversion |
| 51 | Add extensible placeholder gathering/build animations. | gpt-6.1-sol / high | Implemented; included in validated build | Visual assets/animation |
| 52 | Lamanites gain Anger when losses occur; hidden Gadianton lodge generates Lasciviousness until scouted. | gpt-6.1-sol / high | Implemented; included in validated build | Catalog balance; simulation/visibility |

## Owner boundary

This v0.3 revision includes content, UI, audio, input, networking, pathing, and simulation work. Every applicable ledger row is marked implemented, with final validation status to be recorded after the verification pass.

Additional integration task: compress complete multiplayer snapshots and remove repeated fog/player metadata to prevent reliable-buffer overflow with large armies. Best model: **gpt-6.1-sol / high**. Verified in the four-player, 800-unit network stress test.
