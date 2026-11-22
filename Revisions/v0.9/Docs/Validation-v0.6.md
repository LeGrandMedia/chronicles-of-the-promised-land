# v0.6 art integration validation

The final Win64 Development build passed the following checks on October 21, 2026. All checks used the complete 225-image set and the final executable with startup preloading enabled.

| Check | Result |
|---|---|
| Catalog art coverage | 225 of 225 unique PNGs; 155 sprite entries and 261 icon entries |
| Original-source audit | 225 matching prompt receipts and source hashes; all PNGs decode at 1254 by 1254 with alpha |
| Native gameplay and visual checks | 317 passed, zero failures |
| Actual viewport input at 1600 by 900 | 41 cases passed |
| Actual viewport input at 1280 by 720 | 41 cases passed |
| Four local network processes | Host and three clients passed, with 847–890 visible entities |
| Packaged data | All 459 data files match the source project; packaged executable matches the compiled executable |
| Preserved earlier playtests | All 249 protected file hashes remain unchanged |

Viewport tests pass mouse and keyboard events through Unreal's SceneViewport. They cover single-click selection and menus, resource right-click gathering, box selection, hotkeys, training queues, wall placement, rally points, Galavant routes, religion tooltips, garrisoning, and unloading. Captured screens were inspected for artwork, aspect ratio, labels, hotkey visibility, production cards, and resource badges.

The network run checks movement, queued waypoints, construction, training, rallies, Galavant routes, religious counts, private data, Liahona failure/restoration, and match completion. All processes load the complete art set. This is a four-process test on localhost; separate physical LAN computers and adverse network conditions have not been tested in this revision.

Two art integration problems were found and fixed:

1. Large transparent sprite corners could intercept selection of nearby units. Left-click targeting now rejects transparent pixels using a cached alpha mask, while right-click targets retain generous bounds.
2. Clients initially imported the new textures on their first battlefield frame and could miss early state changes. Local controllers now finish importing art before lobby readiness. The unchanged network assertions then passed on all four processes. The failed run is retained in `Evidence/v0.6/network-before-preload/` for comparison.

The renderer imports 380 textures with 221,511,680 resident pixel bytes (211.25 MiB), excluding CPU alpha masks and other engine memory. Loading occurs at startup; there is no per-frame image decoding or file access. This check does not establish a minimum-spec frame-rate guarantee.

Detailed evidence is in `Evidence/v0.6/`, `Validation-v0.6.json`, `ArtCoverage-v0.6.json`, and `ArtReceiptAudit-v0.6.json`. The portable ZIP has a separate checksum and CRC-verification record in `Builds/Chronicles-LAN-Windows.json`.
