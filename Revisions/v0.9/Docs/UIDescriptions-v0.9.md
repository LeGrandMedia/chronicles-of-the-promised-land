# Unit descriptions — v0.9

**Task:** v0.9 beginner-facing unit and action tooltip copy  
**Recommended / actual model:** GPT-6 Luna / GPT-6 Luna

## Changes

Updated all **74 unit descriptions** and **25 faction-specific unit-description overrides** in `Game/Content/Data/catalog.json`. Rewrote all **42 age-choice upgrade descriptions** to match the effects applied by their current `economy_bonuses` and `hero_perk` fields. Stable IDs, numeric balance values, and gameplay fields were left unchanged.

The age-choice text now describes what each selection actually changes: worker gathering, construction and repair speed, Jaredite building-beast income, carrying capacity, unit training, religious-resource generation, or the Lamanite military bonus. Jacob Gadianton has faction-specific text because the Gadianton and House of Laban choices grant different bonuses.

Rewrote **8 active ability descriptions** in the catalog and updated the ability/order tooltip assembly in `RTSSelectionHUD.cpp`. Active ability buttons now use their ability text instead of repeating the hero's age-choice bonus. Tooltips explain Ammonite Prostrate, carrier unloading, and faction-specific conversion results. The 15 order tips now describe what the command does while retaining hotkeys and route controls. `RTSUICommands.cpp` already gates the appropriate ability orders; it had no inaccurate user-facing text to change.

Examples:

| UI item | Updated copy |
|---|---|
| Hagoth's Vessel | “Carries land troops between shores. Move near safe ground to unload them before the vessel is destroyed.” |
| King's Chariot | “Carries land troops and shares vision. Passengers disembark nearby if it is destroyed and safe ground is open.” |
| Gadianton raid move | “Send a raider toward a point; it attacks a visible enemy site and returns stolen cargo.” |
| Nephite conversion | “Soften the Heart: convert an eligible enemy into an Ammonite Covenant Keeper in 5s.” |

## Validation and limits

Offline validation parsed `catalog.json`, confirmed **74 unique units**, non-empty descriptions for all units, **42 age-choice descriptions**, and **25 faction-specific unit descriptions**. No build or runtime test was run; root is handling packaging and validation.

The code contains ability-order branches for Lamoni and Samuel the Lamanite, but neither ID exists in the current unit catalog, so no player tooltip was added for them. Current selection tooltip assembly handles ships, land carriers, and Gadianton Hideouts separately so their unload instructions match movement and landing rules.

The subsequent raid pass also shortened the Band's ability text and corrected its scope: “Raid steals worker cargo or enemy resources. Raid move finds targets along a route.” The final package passed input checks at both viewport sizes; that verifies interaction, not human readability. [Current package evidence](Evidence/v0.9/platform/opening-raids-final-artifacts.json)
