# v0.9 menu and onboarding

Task 3, lore-themed beginner UI. Recommended and actual model GPT-6.2 Sol.

The main menu highlights Practice and offers separate actions for Skirmish, Host LAN, and Join LAN. Join starts with an empty field and asks for the address shown in the host's lobby. The council presents faction strengths beside team and map settings. The name field explains that other players can see the confirmed name, which defaults to **Commander** and appears in the roster on entry. Hosts choose the map, starting resources, speed, Wild supplies, fog, and bot seats. The field guide covers selecting and moving units, gathering, building, recruiting, aging up, and using the Liahona. It uses the active input mode for instructions.

## Optional discoveries

- A brass plate inset opens two brief readings about Nephi's account and Mormon's preservation of the smaller record. The readings paraphrase the source material. [1 Nephi 9](https://www.churchofjesuschrist.org/study/scriptures/bofm/1-ne/9?lang=eng) · [Words of Mormon 1](https://www.churchofjesuschrist.org/study/scriptures/bofm/w-of-m/1?lang=eng)
- A paired-stone inset opens a register of eight carved vessels. Each vessel illuminates two stones. Illuminating all eight reveals a brief account of the crossing. The arrangement references the sixteen stones fashioned at Mount Shelem and two lights carried in each vessel. [Ether 3](https://www.churchofjesuschrist.org/study/scriptures/bofm/ether/3?lang=eng) · [Ether 6](https://www.churchofjesuschrist.org/study/scriptures/bofm/ether/6?lang=eng)

The discoveries live in the main menu and stay optional throughout matchmaking and play. The menu HUD holds the readings and lighting progress for its lifetime.

## Verification targets

Verify both optional discoveries in a native menu, including closing and reopening the vessel register after completion. Check all setup actions still reach their original flows, that the full council remains readable at 16:9 and ultrawide widths, and that the guide can be opened from the main menu and a running match. Use the desktop touch preview and a physical Android device to verify select, box select, commands, camera gestures, command cancellation, and keyboard entry. Record artifact, viewport, input device, and observed behavior for each pass.

A source check found the expected lobby and touch actions in the HUD and their corresponding controller handlers. It also confirmed all five lore actions and verified that the menu, relic panel, and touch strips have nonoverlapping calculated bounds at 1280×720, 1600×900, 2400×1080, 1366×768, and 1280×800. Packaged screenshots and actual input results are tracked in the v0.9 development ledger.

The final Windows menu capture at 1600×900 shows a clean distance-field title, the four match choices, the etched artifacts, and the faceted dusk landscape. The initial capture exposed a visibly enlarged glyph atlas. The Slate capture exposed a missing title in the runtime font path. [Final menu](Evidence/v0.9/rendering/menu-v09-final.png) · [Initial menu](Evidence/v0.9/rendering/menu-v09.png) · [Slate capture](Evidence/v0.9/rendering/menu-v09-slate-capture.png)

In a development build, run the game with `-RTSLoreTest` from the ordinary menu. The probe checks actual HUD hit rectangles and follows Practice priority, empty and typed LAN addresses, name publication and confirmation, and the field guide. It opens Nephi's plate and switches to Mormon's, then closes and reopens the vessel register after lighting all eight pairs. It reports `RTS_LORE_TEST PASS` and writes menu, Join LAN, small-plates, vessel, lobby, and guide captures under the game's saved screenshots directory.

The initial Windows lore probe passed at 1600 × 900. The first run reached the pass marker and timed out during shutdown with forced log flushing enabled. [Clean initial log](Evidence/v0.9/rendering/lore-test-v09-distance-field.log) · [Small plates](Evidence/v0.9/rendering/lore-small-plates-v09.png) · [Sixteen stones](Evidence/v0.9/rendering/lore-vessels-v09.png)

The updated November 22 Win64 Development package passed the full probe at **1600 × 900** and **1280 × 720**, both with exit code 0. The captures show the prioritized Practice option, the empty Join field, and **Commander** in both the name field and roster before Ready. The verified settings and native notice results are recorded in the [beginner-facing audit](BeginnerUIAudit-v0.9.md). [1600 × 900 log](Evidence/v0.9/input/beginner-ui-fixed-1600x900/lore-ui.log) · [1280 × 720 log](Evidence/v0.9/input/beginner-ui-fixed-1280x720/lore-ui.log) · [Main menu](Evidence/v0.9/input/beginner-ui-fixed-1600x900/beginner-menu-v09.png) · [Join LAN](Evidence/v0.9/input/beginner-ui-fixed-1280x720/beginner-join-lan-v09.png) · [Skirmish setup](Evidence/v0.9/input/beginner-ui-fixed-1280x720/lobby-v09.png)

The subsequent Windows package publishes the confirmed name separately from the Ready, team, and faction controls. The 1600 × 900 menu probe and all four processes in the loopback LAN match passed on that package. [Menu log](Evidence/v0.9/input/name-only-fixed-1600x900/lore-ui.log) · [Lobby capture](Evidence/v0.9/input/name-only-fixed-1600x900/lobby-v09.png) · [LAN host report](Evidence/v0.9/network/name-only-fixed/humans/network-host.json)
