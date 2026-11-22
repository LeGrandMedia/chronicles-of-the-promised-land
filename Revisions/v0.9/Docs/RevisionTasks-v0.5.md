# v0.5 Playtest Checklist

This checklist records the 13 v0.5 playtest requests supplied for this revision. Implementation is complete. Native, viewport and network verification is recorded in [Validation-v0.5](Validation-v0.5.md); combat findings are in [CombatPlaytest-v0.5](CombatPlaytest-v0.5.md).

| # | User request | Best-suited model | Status | Owner area |
|---:|---|---|---|---|
| 1 | Laban supports Drunken Concubines instead of granting a general military bonus; its age choice and nearby aura improve concubines. | gpt-6-luna / medium for catalog; gpt-6.1-sol / high for runtime | Implemented; verified | Hero perks, auras |
| 2 | Let players set a Galavant route for Traders and inherit it when another Trader is trained. | gpt-6.1-sol / high | Implemented; verified | Orders, production |
| 3 | Show the selected building’s queue at the bottom and a corner queue with accurate busy status. | gpt-6.1-sol / high | Implemented; verified | HUD, production |
| 4 | Add three Storehouse economic upgrades to each faction, using the specified bonus types and faction-only descriptions. | gpt-6-luna / medium for catalog; gpt-6.1-sol / high for application | Implemented; verified | Catalog, economy |
| 5 | Give the Seat of the People distinct economic and military rally points. | gpt-6.1-sol / high | Implemented; verified | Commands, rally |
| 6 | Show religious-stat bonuses and counts of religious units and structures on hover. | gpt-6.1-sol / high | Implemented; verified | HUD, civic resources |
| 7 | Reduce map tree density and keep Jaredite animals from getting stuck. | gpt-6.1-sol / high | Implemented; verified | Maps, navigation |
| 8 | Set Curelom, Cumelom, and Elephant population cost to 0; enforce 10 living plus queued animals per flock and a separate 10-animal HQ bootstrap limit. | gpt-6-luna / medium for catalog; gpt-6.1-sol / high for runtime | Implemented; verified | Catalog, population, navigation |
| 9 | Remove depleted resource art and collision, including depleted Hills of Ore. | gpt-6.1-sol / high | Implemented; verified | Resource depletion, rendering |
| 10 | Let Conspiring Civilians gather berries, hunt, and use allied or enemy farms/flocks while forbidding construction of their own farms/flocks. | gpt-6.1-sol / high | Implemented; verified | Worker orders, economy |
| 11 | Add Q as the build-menu hotkey. | gpt-6.1-sol / high | Implemented; verified | Input, HUD |
| 12 | Show both Lamanite Temple upgrades, Order of the Nehors and By the Pride of Their Hearts, in the Rameumptom research menu. | gpt-6-luna / medium for catalog; gpt-6.1-sol / high for UI | Implemented; verified | Catalog, research UI |
| 13 | Provide a combat playtest report. | gpt-6.1-sol / high | Implemented; verified | Combat balance |

Content and documentation were delegated to GPT-6 Luna; simulation and interface subtasks used GPT-6.1 Sol, with integration handled by the main agent. Each row retains its best-suited model label.
