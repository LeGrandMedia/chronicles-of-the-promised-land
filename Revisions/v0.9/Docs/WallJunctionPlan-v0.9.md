# Wall corners and placement — v0.9

GPT-6.2 Astra was recommended for renderer integration. GPT-6.2 Sol was recommended and used for placement tests, interaction tests, and the builder-escape fix. The November 22 Windows development package verified the mitered bends. The refreshed Android APK and Gate-District Windows installer include that implementation; physical Android rendering and installation of the refreshed Windows artifact remain unverified. [Artifact hashes](Evidence/v0.9/platform/gate-district-artifacts.json)

Queued wall turns preserve the shared corner's original heading. The server's collision circles overlap at the join, and the original meshes left visible gaps. The client now joins the walls using their authored masonry, timber stakes, palette, and height. A completed two-leg bend clips the incoming and outgoing models along a shared angle bisector to remove the unused straight tip.

## Rendering and interaction

- Adjacency uses only filtered snapshots: same owner, definition and model, at queue spacing `2 * Radius + 0.15`. A spatial grid limits pair searches. A join requires one segment aligned to the new line and a measurable gap in the authored cross-section at 45 cm above ground. Straight runs receive no extra geometry.
- A completed, unambiguous two-leg bend shows the clipped mesh in place of the full corner segment. An unfinished corner, a third branch, a missing incoming neighbor, or mismatched remembered states uses the full corner with the outgoing half-wall join. A nearby gate counts as a branch for this decision.
- One procedural component submits the junction triangles. Sorted snapshot descriptors cache geometry across camera movement, selection, and snapshot ordering. Changes to position, heading, owner, type, model, progress, gate status, or remembered state rebuild it.
- Exact triangle picking resolves to the real corner wall. Its projected bounds follow the displayed geometry. Picking rejects newly removed, remembered, or gate endpoints before the next renderer update.
- Height follows the lesser construction progress of the connected walls. The base renderer also updates on the final change to 100%, fixing a nearly finished mesh that could remain slightly short.
- Either remembered endpoint dims the join and disables its picks. Removing an endpoint from the filtered snapshot removes the junction and its bounds. Gates are excluded, preserving their openings.

## Measured evidence

The initial half-wall junction passed **87/87** rendered assertions with native exit 0. The fixture uses authoritative `wall_line` orders and filtered server snapshots, with construction progress set to fixed values for geometry comparisons. [Initial results](Evidence/v0.9/rendering/wall-junction-first/wall-junction-tests.json)

| Wall style | Previous 45° gap | Previous 90° gap | Remaining projected separation after joining |
|---|---:|---:|---:|
| Nephite stone | 38.04 cm | 172.89 cm | 0 cm |
| Lamanite timber | 35.75 cm | 175.22 cm | 13.35 cm overlap, matching its authored seam |
| Jaredite cement | 10.51 cm | 75.89 cm | 0.40 cm overlap, matching its authored seam |

The probe verified submitted geometry, actual on-screen drawing, exact integer-pixel picks at two camera angles, gate upgrades on both endpoints, unrelated owners/types, progress at 35%, 99% and 100%, remembered appearance, missing-client-neighbor cleanup while that neighbor still exists on the server, and geometry reuse across rotation and reversed snapshot order. Gate openings remained 214.52 cm for ordinary walls and 111.03 cm for cement.

The initial screenshots show the joined walls with the full corner segment still extending past each bend. [Stone](Evidence/v0.9/rendering/wall-junction-first/wall-junction-nephites.png) · [Timber](Evidence/v0.9/rendering/wall-junction-first/wall-junction-lamanites.png) · [Cement](Evidence/v0.9/rendering/wall-junction-first/wall-junction-coriantumr.png)

The mitered renderer passed **102/102** assertions with native exit 0. The 45 cm cross-section met both neighboring segments on each completed bend. At 90°, the probe found integer pixels that had selected the original unused tip and now hit empty space. Picks on the retained incoming leg and the outgoing wing resolved to the same real corner wall. A third gate branch and an incoming segment withheld from the client each restored the full corner and outgoing half-wall join. [Mitered results](Evidence/v0.9/rendering/wall-miter-first/wall-junction-tests.json)

| Right-angle wall style | Reduction along the old straight axis | Remaining reach past the corner center |
|---|---:|---:|
| Nephite stone | 172.887 cm | 34.613 cm |
| Lamanite timber | 199.600 cm | 14.320 cm |
| Jaredite cement | 76.287 cm | 31.413 cm |

Both reviewers inspected the three mitered screenshots. Each shows both completed bends without a protruding straight segment. The HUD crops part of the straight comparison wall. [Stone](Evidence/v0.9/rendering/wall-miter-first/wall-junction-nephites.png) · [Timber](Evidence/v0.9/rendering/wall-miter-first/wall-junction-lamanites.png) · [Cement](Evidence/v0.9/rendering/wall-miter-first/wall-junction-coriantumr.png)

## Builder escape and remaining boundary

The preceding authoritative placement fixture found a builder trapped inside a just-completed wall when existing escape directions all led into its neighbor. Extending the checked escape arc through ±90° resolved both reproduced stalls in 42.9 simulated seconds. [Placement and gate traversal: 216/216](Evidence/v0.9/native/wall-placement-fixed/wall-placement-tests.json). The [full native replay](Evidence/v0.9/native/wall-escape-regression/self-tests.log) passed all 19 reports after that navigation change.

The gate grid miss was reproduced and corrected with clearance-checked friendly crossings, passing 46/46 focused checks. Friendly traversal remains wider than the visible aperture, so art clipping still needs a separate correction. See [gate traversal evidence and limits](GateTraversal-v0.9.md). Physical mobile rendering and cross-device input remain unverified.
