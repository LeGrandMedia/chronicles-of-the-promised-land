# Current implementation tasks

These fifteen tasks track the current playable-build effort and requested follow-up separately from the original 407-action development and asset plans. Model/effort labels are suitability recommendations, not claims about which model executed each task. Historical plan checkboxes and phase gates are unchanged; the original 407 actions are not being marked complete by this runtime report.

| ID | Task | Recommended model | Effort | Status |
| --- | --- | --- | --- | --- |
| P001 | Build the Unreal 5.3 project and resolve compiler or toolchain issues without changing protected source notes. | gpt-6.1-sol | high | Verified: Chronicles Win64 Development target compiled successfully. |
| P002 | Complete the visible host/join menu for direct IPv4 LAN play, including clear connection errors and 1v1/2v2 team setup for up to four players. | gpt-6.1-sol | high | Partially verified: two- and four-process packaged loopback host/join passed; join-failure test returned to menu; menu visual review passed at 1600×900. Physical-LAN test remains pending. |
| P003 | Integrate the authoritative 0.1-second game simulation with client-issued, server-validated orders and synchronized state. | gpt-6-astra | high | Partially verified: packaged 2- and 4-process loopback sessions exercised synchronized gameplay; 43 native/scenario tests passed. Physical-LAN networking remains pending. |
| P004 | Implement the playable RTS loop: camera, selection, movement, gathering, construction, production, combat and victory feedback. | gpt-6-astra | high | Partially verified: corrected scenario tests passed 17/17; loopback covered movement, training, construction, and winner behavior. |
| P005 | Deliver a runnable sandbox match with representative terrain, units, buildings and readable HUD controls. | gpt-6.1-sol | high | Verified for current build scope: packaged launch, solo sandbox path, 1600×900 gameplay/menu visual review, and 17/17 scenario tests passed. Original production plan remains open. |
| P006 | Package a Windows Development build at `Builds/Windows/Chronicles.exe` and confirm that a clean launch reaches the game. | gpt-6-luna | high | Verified: compile, cook (3,430 packages, zero cook errors/warnings), UAT stage/package/archive, bootstrap launch, and tests all succeeded. |
| P007 | Verify two packaged clients can host and join over a private LAN, including host exit and join failure behavior; record evidence and user-controlled firewall guidance. | gpt-6.1-sol | high | Two- and four-process packaged loopback verified on one computer, including join-failure return and match winner; physical-computer LAN remains unverified. |
| P008 | Review menu and in-match presentation for legibility, input feedback, and graceful handling of disconnects and invalid orders. | gpt-6-luna | high | Partially verified: menu/gameplay visual review passed at 1600×900; join-failure handling returned to menu. Broader disconnect/invalid-order review remains open. |
| P009 | Harden and validate pointer input: single-click selection, right-click contextual orders, drag-selection, and double-click behavior, including UI/minimap boundaries and event-order edge cases. | gpt-6.1-sol | high | Verified for runtime scope: viewport input passed 15/15 cases at both 1600×900 and 1280×720, including selection, gathering/delivery, build input, hotkeys, and rally markers. |
| P010 | Add user-facing hotkey settings for rebinding build and idle-worker shortcuts, including conflict handling, persistence, and reset. | gpt-6.1-sol | high | Verified for runtime scope: customization suite passed 41/41, covering rebinding, conflicts, saved preferences, defaults, and override safety. |
| P011 | Add discoverable idle-worker selection controls and hotkeys, including the idle count and select-all interaction. | gpt-6.1-sol | high | Verified for runtime scope: viewport tests exercised next/all idle shortcuts and counter clicks at both supported test resolutions. |
| P012 | Implement rally destinations for every unit-producing building; newly trained workers gather at valid resource rallies and other units move to the rally point. | gpt-6.1-sol | high | Verified for runtime scope: rally suite passed 6/6, including one assertion training through all eight producer buildings; viewport and four-process network checks also confirmed rally interaction/replication. |
| P013 | Present Wealth as “Precious Things” in resource labels and legacy help/notice text while preserving stable resource IDs and the existing trader loop. | gpt-6-luna | medium | Verified for runtime scope: customization tests confirmed the default label and stable resource metadata ID. |
| P014 | Add an editable display-name registry for catalog definitions, factions, resources, and named abilities, with loose runtime overrides and legacy-text resolution. | gpt-6-luna | medium | Verified for runtime scope: customization suite passed 41/41, including stable unit/faction/resource IDs and runtime naming behavior. |
| P015 | Add a visual-asset adapter and external manifest for replaceable unit, building, and resource artwork, with procedural fallback. | gpt-6.1-sol | high | Verified for runtime scope: visual suite passed all 6 checks for PNG import, retention, hit bounds, unknown/missing fallback, and restored defaults. |

## Evidence and boundaries

The v0.2 feedback build is verified in the runtime scope below. It does not complete the original 407-action production and asset plans or establish physical-PC LAN or rendered GPU performance.

- The compiled source and packaged executable matched. Build evidence is recorded in [v0.2 verification](Evidence/2026-10-20/feedback-v0.2/build-verification.json); the package SHA-256 is `a0e9fb8de57a95c5ea84de57f3d10e2e2863c043eb6c84f071984a1e5aaad876`.
- Native self-tests passed 26/26, gameplay scenarios passed 17/17, customization tests passed 41/41, rally tests passed 6/6, and visual asset tests passed 6/6.
- Viewport input passed 15/15 cases at each of 1600×900 and 1280×720. At 1600×900, the pointer test observed worker gathering and delivery of 10 Food. The same suite checked single-click selection, drag selection, build card/shortcut input, idle-worker actions, and a replicated rally marker.
- Four packaged processes passed the loopback network test, including movement, training, construction, replicated rally, Liahona failure/recovery, and winner handling. Each saw more than 800 entities (host/clients maximum visible: 839/830/839/830); this remains same-computer loopback evidence, not a physical LAN test.
- LAN target is direct IPv4 over UDP 7777. Allow the game through Windows Firewall on a Private network when prompted; both machines must be on a LAN that permits peer-to-peer traffic. No automatic firewall edits are made. Host exit ends the match.
- The current art is procedural and sound cues synthesized. There are no recorded voice performances, Steam/public internet matchmaking, saved games, or reconnect flow. Physical-PC LAN and rendered GPU frame rate remain unverified/unmeasured.

See [RuntimeBuildReport.md](RuntimeBuildReport.md) and [KnownIssues.md](KnownIssues.md). This implementation checklist is not a replacement for the acceptance checks in the original development and asset plans.

## User-feedback follow-up

P009–P015 track usability and extensibility work verified for the v0.2 runtime scope above. These tasks are separate from the original 407-action plan. Model/effort labels are recommendations for suitability and do not claim that those models executed the tasks.
