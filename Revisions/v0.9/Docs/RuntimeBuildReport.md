# Runtime build and verification report

Updated October 20, 2026. This records development evidence only; it is not a physical-LAN acceptance report or a rendered-GPU performance report.

## v0.2 feedback verification

The v0.2 source and packaged executable match (`source_matches_package: true`). The package SHA-256 is `a0e9fb8de57a95c5ea84de57f3d10e2e2863c043eb6c84f071984a1e5aaad876`.

- Native self-tests: 26/26; gameplay scenarios: 17/17; customization: 41/41; rally: 6/6; visual assets: 6/6.
- Viewport input: 15/15 cases at 1600×900 and 15/15 at 1280×720. The 1600×900 pointer run observed a worker gather and deliver 10 Food. Both runs covered viewport interaction, build controls, idle-worker selection, and rally UI.
- Four packaged processes passed the same-computer UDP loopback match. Host and three clients all passed movement, training, construction, rally replication, Liahona failure/recovery, and winner reporting. Snapshot maxima were 839, 830, 839, and 830 visible entities, respectively.
- The latest packaged visual-asset tests cover import, garbage-collection retention, scaled hit bounds, unknown/missing-art fallback, and original-state restoration.

Evidence: [build and package verification](Evidence/2026-10-20/feedback-v0.2/build-verification.json), [native tests](Evidence/2026-10-20/feedback-v0.2/self-tests.json), [scenario tests](Evidence/2026-10-20/feedback-v0.2/scenario-tests.json), [customization tests](Evidence/2026-10-20/feedback-v0.2/customization-tests.json), [rally tests](Evidence/2026-10-20/feedback-v0.2/rally-tests.json), [visual tests](Evidence/2026-10-20/feedback-v0.2/visual-tests.json), [1600×900 viewport input](Evidence/2026-10-20/feedback-v0.2/input-1600x900.json), [1280×720 viewport input](Evidence/2026-10-20/feedback-v0.2/input-1280x720.json), and [host](Evidence/2026-10-20/feedback-v0.2/network-host.json), [client 1](Evidence/2026-10-20/feedback-v0.2/network-client1.json), [client 2](Evidence/2026-10-20/feedback-v0.2/network-client2.json), and [client 3](Evidence/2026-10-20/feedback-v0.2/network-client3.json) loopback reports.

Physical-PC LAN behavior remains unverified. Rendered GPU frame rate has not been measured. The prior verified-build record follows for historical context.

## Previous verified-build baseline

- The `Chronicles` Win64 Development game target compiled successfully.
- Unreal content cooking completed with 3,430 packages and zero cook errors or warnings.
- Native self-tests passed 26/26 and scenario tests passed 17/17 (43/43 combined). The test executable exited 0.
- Two packaged game instances connected on one computer over loopback using UDP 7788. Movement, unit training, construction, Liahona failure and apology recovery, and winner/end behavior passed in that run.
- Four packaged processes completed a loopback network stress match: host and three clients joined, and all four exercised movement, training, construction, Liahona failure/recovery, and winner handling. Snapshot counts were host 190 (max 841), client 1 183 (max 832), client 2 185 (max 832), and client 3 184 (max 841). Population was seeded at 30 seconds; clients deliberately resigned at 46 seconds, and the match ended at 46.5 seconds. The normal 60-second headquarters-defeat timer was not reached. All four reported winner 0; there were no engine errors or overflow.
- The join-failure test logged `RTS_JOIN_FAILURE_TEST_PASS` and returned the client to the menu.
- Root and client-agent visual review passed for the menu and gameplay at 1600×900.
- The corrected gameplay scenario suite passed 17/17 after fixture fixes.
- Final source compile and UAT stage/package/archive completed successfully. The packaged top-level `Chronicles.exe` launched; self/scenario tests passed 26/26 and 17/17, and the test process exited 0.
- A headless four-player CPU simulation used 800 canonical Man of the Sword units (200/player) for 100 simulation steps/10 simulation seconds. All players moved and 622 units survived. Invariants passed; median step time was 16.5632 ms, maximum 46.9588 ms, total wall time 1.76985 s. This measures the headless CPU simulation on this host, not rendered GPU frame rate.

## Still unverified or unmeasured

- GPU/rendered frame-rate performance: **not measured.**
- Physical second-computer private-LAN test: **pending.**

The four-process run used packaged clients on one machine; it is not evidence that two physical PCs can connect over a LAN. The visual and CPU simulation results do not establish GPU frame rate. See [KnownIssues.md](KnownIssues.md) for user-facing limitations. Test artifacts are in [runtime evidence](Evidence/2026-10-20/runtime/): [self-tests](Evidence/2026-10-20/runtime/self-tests.json), [scenario tests](Evidence/2026-10-20/runtime/scenario-tests.json), [CPU stress results](Evidence/2026-10-20/runtime/stress-tests.json), the [four-client host report](Evidence/2026-10-20/runtime/network-host.json), [client 1](Evidence/2026-10-20/runtime/network-client1.json), [client 2](Evidence/2026-10-20/runtime/network-client2.json), [client 3](Evidence/2026-10-20/runtime/network-client3.json), and [menu](Evidence/2026-10-20/runtime/menu.png), [lobby](Evidence/2026-10-20/runtime/lobby.png), and [gameplay](Evidence/2026-10-20/runtime/gameplay.png) screenshots, plus associated test logs.
