# Beginner-facing UI audit — v0.9

GPT-6 Luna was recommended and used for the November 22 source and screenshot audit. GPT-6.2 Sol was recommended and used for implementation and tests. The design calls for plain-language mechanics and optional lore for Mormon and ex-Mormon players, including people new to RTS games. [Design contract](../../../book%20of%20mormon%20RTS/01%20Foundation/Design%20Contract.md)

| Audit finding | Implemented behavior |
|---|---|
| Join LAN prefilled the local computer address, so typing a host address appended to that value. | Fresh processes start with an empty field. The placeholder directs the player to the address in the host's lobby, and Join stays disabled until the field contains text. Entries survive Back and reopening. An address used for a join attempt stays available if the player returns from that connection in the same process. |
| The default name came from the operating system, and the field gave no indication that other players could see it. | The default is **Commander**. Enter, native keyboard acceptance, and Ready confirm and save a custom name locally. When the lobby first shows the server's numbered placeholder, the controller publishes the confirmed name with the current faction, team, and readiness. Team and faction changes use the confirmed name while a draft is open. Escape and native keyboard cancellation restore the confirmed value. The privacy hint is visible beside the field. |
| Skirmish was given the first, highlighted menu action. | **Practice** is first and highlighted with a short description. Skirmish still explains that it adds a computer opponent. |
| The label **Land** suggested terrain, and its explanation required hover. | **Wild supplies** controls how much each tree, ore deposit, berry patch, and wild animal can provide. The explanation is visible under the settings for touch and mouse users. |
| The Liahona notice gave a recovery phrase without explaining the chat trigger or stat loss. | The notice identifies the chat trigger and tells the player to send `sorry`. A deduction displays the amount actually removed and the faction's stat label. A duplicate submission during the cooldown keeps the prior loss notice visible. The House of Laban stat uses **Drunkenness** in the HUD and notice. Its separately named ability uses **Seduce**. |

## First packaged Windows pass

The native menu probe passed at **1600 × 900** and **1280 × 720**. It verified the new order of Practice and Skirmish, fresh address entry, disabled empty Join, and retaining a typed address through Back. The captures show the host-lobby placeholder, privacy hint, and Wild supplies explanation at rest. [1600 × 900 log](Evidence/v0.9/input/beginner-ui-1600x900/lore-ui.log) · [1280 × 720 log](Evidence/v0.9/input/beginner-ui-1280x720/lore-ui.log) · [Main menu](Evidence/v0.9/input/beginner-ui-1600x900/beginner-menu-v09.png) · [Join LAN](Evidence/v0.9/input/beginner-ui-1280x720/beginner-join-lan-v09.png) · [Lobby](Evidence/v0.9/input/beginner-ui-1280x720/lobby-v09.png)

The first lobby screenshots show **Commander** in the field and **Player 1** in the roster. Reviewing that mismatch led to the confirmed-name sync on lobby entry.

The first native run passed **53/54** core checks, including the other five faction notices, clamped loss, and cooldown. The House of Laban notice failed because the text resolver treated the shared source phrase **Drunkenness** as an ability name. The fix gives resource and devotion terms priority in unqualified text. The scoped ability name remains available through its content ID. [First native report](Evidence/v0.9/native/beginner-ui-miter/core-v07-tests.json)

## Verified Windows follow-up

The updated menu probe passed at **1600 × 900** and **1280 × 720**, both with exit code 0. Both lobby captures show **Commander** in the name field and roster before Ready. The probe verified that the confirmed-name sync preserves faction, team, and readiness on entry. It also verified that changing teams keeps the draft private, Escape restores the confirmed name, and Ready publishes and saves the edited name. The original saved display-name key was restored after the run. [1600 × 900 log](Evidence/v0.9/input/beginner-ui-fixed-1600x900/lore-ui.log) · [1280 × 720 log](Evidence/v0.9/input/beginner-ui-fixed-1280x720/lore-ui.log) · [1600 × 900 lobby](Evidence/v0.9/input/beginner-ui-fixed-1600x900/lobby-v09.png) · [1280 × 720 lobby](Evidence/v0.9/input/beginner-ui-fixed-1280x720/lobby-v09.png)

The updated native suite exited 0 and produced 19 reports. Core passed **54/54**, including the actual server notice “Lost 10 Drunkenness” at a measured 10-point deduction. Customization passed **75/75**, including the distinction between that stat label and the **Seduce** ability. [Core report](Evidence/v0.9/native/beginner-ui-fixed/core-v07-tests.json) · [Customization report](Evidence/v0.9/native/beginner-ui-fixed/customization-tests.json) · [Native log](Evidence/v0.9/native/beginner-ui-fixed/self-tests.log)

The two-viewport follow-up used the November 22 Win64 Development game binary with SHA-256 `7DA5726001DD599F4E48DCFC4CD3D1B9E4F8AE15C9403DDA5D301838CF0B1016`.

## LAN readiness follow-up

The first automatic name update sent the faction, team, and Ready state from its client snapshot. In the four-client loopback test, a delayed name update carried an older Ready value and blocked match start. The server now accepts the automatic name separately and changes only its own numbered placeholder. A confirmed name wins even if the player explicitly chose the same text as the placeholder. [Original host log](Evidence/v0.9/network/name-ready-race/Test-LAN-v09-humans-host.log)

All four processes passed the unchanged loopback match probe after the correction. The native suite produced 19 passing reports, and core passed **57/57**, including three new checks for Ready, faction, team, and explicit name preservation. The 1600 × 900 menu retest exited 0 and showed the confirmed name in the roster before Ready. These results use Win64 Development game binary SHA-256 `60AF3B546A2D7C8204D07121054C7AF2DA98F91EF9837B539DED1DADCF1ECC82`. [Loopback host report](Evidence/v0.9/network/name-only-fixed/humans/network-host.json) · [Core report](Evidence/v0.9/native/name-only-fixed/core-v07-tests.json) · [Menu log](Evidence/v0.9/input/name-only-fixed-1600x900/lore-ui.log) · [Lobby capture](Evidence/v0.9/input/name-only-fixed-1600x900/lobby-v09.png)

The evidence comes from automated desktop interaction and visual review. Human usability and physical mobile text entry were outside this pass.
