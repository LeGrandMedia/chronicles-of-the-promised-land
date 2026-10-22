# v0.8 Implementation Status

All 21 current user requests and the eight previously tracked implementation gaps are implemented and validated in the v0.8 packaged build. The reported checks include 693 native assertions, 67 rendered-input checks at each of two resolutions, and four-process localhost multiplayer. See [[Validation Report]] for evidence and limits; individual rows refer to validation coverage, not human balance playtesting.

| Previously tracked gap | v0.8 resolution | Status |
|---|---|---|
| Dramatic Nephite HP curve | Favor follows the nonlinear 1 HP / base HP / 100× curve at 0 / 50 / 100 Favor. | Validated in v0.8 |
| Conversion hostility and capacity | A target must have been hostile for 60 seconds; capacity is checked when channeling starts and when conversion completes. | Validated in v0.8 |
| Gadianton fallback gathering | Gadianton laborers gather at 35% of the ordinary rate. | Validated in v0.8 |
| Gadianton HQ/camp concealment | Seat of the People and Encampments conceal after five seconds without damage. | Validated in v0.8 |
| Jaredite tier rules | Foundations remain locked to their eligible beast tier; any beast tier can repair a completed structure without changing its tier. | Validated in v0.8 |
| Laban gates | The Laban faction can research and build gates. | Validated in v0.8 |
| Priestcrafter aura/occupancy | A Priestcrafter can occupy its completed Temple for the stronger aura, or provide the smaller aura outside. | Validated in v0.8 |
| Teancum Night Raid | The catalog exposes Night Raid to the UI and ability system. | Validated in v0.8 |

## Separate future work

Save/reconnect persistence, physical multi-PC networking, Steam/internet deployment, human balance, and production presentation remain future validation or development work; they are not among the eight resolved rule gaps. See [[Developer Handoff]].

Earlier descriptions of old catalog text or v0.7 behavior are historical evidence only. Follow the current system notes and v0.8 catalog for active rules.
