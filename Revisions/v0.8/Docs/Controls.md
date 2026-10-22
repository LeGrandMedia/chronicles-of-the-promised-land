# Controls and hotkeys

Hotkeys are editable per player in **Menu > Customize Hotkeys**. Select a row and press a key; Ctrl, Shift, and Alt modifiers are supported. Backspace clears a binding, Escape cancels capture, conflicting bindings in the same group swap, and **Reset Defaults** restores the defaults. Build-menu and action hotkeys use stable action IDs, so changing display names does not change bindings. Defaults are saved locally for each player.

## In-game controls

- **Q:** open construction for a selected builder; Escape closes it. While construction is open, configured build-card shortcuts choose structures.
- **Right-click:** issue the applicable order, such as move, gather, repair, board, trade, convert, or attack. At the Seat of the People, right-click sets the economic rally; Y sets the military rally. Right-click Wild Game with a laborer to hunt. Right-click an eligible enemy economic building with a Band of Kishkumen to raid it.
- **Y:** set a producer's military rally, or set a Gadianton Storehouse's **Raid Rally**. On a Storehouse, click an enemy economic site to route new Robbers there; click open ground to set a raid-move destination. Robbers return cargo to that Storehouse and repeat the route. **Shift+Y** sets an eligible headquarters' economic rally for new laborers, builder beasts, traders, or sheep; left-click a resource while placing it to assign new workers there.
- **G:** set a Galavant route. At a market, it applies to new traders; on a trader, it edits that trader's route. Click the first waypoint, Shift-click to append waypoints, and press Escape to finish.
- **V:** order a Band of Kishkumen to raid the first eligible enemy economic site it sees, then return to the nearest friendly resource drop-off. It steals cargo from laborers before their drop-off and plunders economic sites rather than damaging them.
- **Bribe:** the Lawyer's ability reveals the Lawyer while channeling. Enemy units become eligible for conversion after 60 seconds of match time. Conversion capacity is reserved when a channel begins; ensure there is room for the converted unit.
- **Shift+click a unit card:** queue five units, subject to resources, population, and queue space. Shift with a world command appends that order.
- **Queues:** the selected building's queue appears at the bottom of its panel with item progress and queue count. The screen-corner production queue shows up to 12 items across your producers; hover for details and right-click an item to cancel it. Canceled units and research receive a full refund.
- **Walls:** select a builder, choose Works of Defense or Cement Wall, then click both endpoints. Hold Shift while choosing another endpoint to continue the chain. Escape finishes placement. Accepted segments are paid for and appear immediately as scaffolds. Workers build in sequence; builder beasts can work on several nearby segments at once. Select a completed segment to upgrade it to a gate; your team can pass through.
- **Carrier passenger limits:** Chariots carry 6 population, Elephants 10, Jaredite Barges 12, other boats 10, and Gadianton hideouts 5 population of robber units. These limits are separate from the carrier's own population cost.
- **Workforce counts:** click the worker count under a resource to cycle through its assigned workers.
- **Production cards:** use the shortcut printed on the card. Hover for cost and population; right-click a queue card to cancel it. Age-locked cards display the required Roman numeral.
- **Rameumptom research:** the Lamanite Temple offers Order of the Nehors and By the Pride of Their Hearts. The latter requires the former.
- **Idle workers:** Period (`.`) selects the next idle worker; Ctrl+Period selects all. Ctrl-click the Idle Workers counter selects every idle worker. Jaredite beasts are excluded.
- **Selection portraits:** Shift-click an individual portrait to remove that unit; Ctrl-click to isolate it. Page controls support large selections.
- **Selection and camera:** Shift-click adds or removes units. Arrow keys/WASD pan, mouse wheel zooms, Shift+wheel rotates, and Space returns to headquarters. The minimap rotates with the camera.
- **Control groups:** Ctrl+0–9 assigns, 0–9 selects, Shift+0–9 appends, and Alt+0–9 selects and centers on a group. Double-tap a number to center on the average position of living units in that group.
- **Other commands:** A/S/H/P attack-move, stop, hold, or patrol when supported; C/R/E invoke a supported conversion, raid, or unit ability. Attack-move applies to units, not structures. Enter opens chat; Escape cancels or opens a menu; F1 opens the field guide.
- **Religious stat:** hover the religious-stat display in the top bar to see religious-unit and structure counts, generation and decay, and faction effects.

## v0.8 faction changes

The Laban Winery is its primary religious building and trains Drunken Concubines. It generates Drunkenness; an Age III upgrade adds 3 Food/s. The Great and Spacious Building unlocks Age IV, is limited to one, grants the Concubine aura, and produces Drunkenness at five times a Winery’s rate. Concubines speed nearby laborer gathering, and their visible aura shows its area. Chief Captains and King's Chariots unlock in Age III; One of the Fifty unlocks in Age IV.

All religious units automatically seek eligible conversion targets when idle. Converted units are permanently immune to further conversion. Nephite Soften the Heart is innate; Mighty Change grants Ammonites Prostrate (12-second cooldown, five-second martyrdom window). Lamanite Stiffen the Neck creates Zoramite Captains. Selected ranged attackers show their range; unit panels show effective attack, attack interval, ranges, splash radius, counters, and passive income.

## Names and art

Display labels never change mechanical IDs. Update defaults in `Game/Content/Data/names.json`; loose runtime overrides can be edited without recompiling. Artwork is described by `Game/Content/Data/visuals.json` and image files under `Game/Content/Data/Art/`. See [Naming](Naming.md) and [Art Replacement](ArtReplacement.md).
