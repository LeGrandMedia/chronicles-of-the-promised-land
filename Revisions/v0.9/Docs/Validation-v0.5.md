# v0.5 validation

The isolated Windows build implements all 13 revision requests. The earlier root build, v0.3 and v0.4 were preserved: SHA-256 comparison found no changes in the 181 protected source, content, configuration, launcher, executable and package files recorded before copying.

## Results

- **317 native assertions passed, 0 failed**, including economy, construction, navigation, population limits, religion, queues, rallies, customization and combat.
- **41 actual viewport input checks passed at 1600×900 and again at 1280×720.** Tests send normal mouse and keyboard input; they click both Rameumptom upgrades, configure and inherit a Market route, use separate Seat rallies, inspect/cancel a selected building queue, open construction with Q, and hover Anger to verify live bonuses and religious counts. Screenshots were reviewed for readable layout.
- **Four local multiplayer processes** exercise a host plus three clients using different factions. They cover real order RPCs, more than 800 visible entities under load, snapshot compression, hidden-unit/privacy rules, Market routes, split rallies, busy-building status and religious information. These are localhost network tests; separate physical LAN machines and router/firewall configurations have not been tested.
- **12 timed native combat engagements**, plus targeted implementation assertions, are recorded in [CombatPlaytest-v0.5](CombatPlaytest-v0.5.md). Outcomes expose balance concerns; passing regression checks does not mean every matchup is balanced.

## Bugs caught during this revision

Both Rameumptom researches existed as definitions but were missing from the Lamanite faction research whitelist. The definitions, whitelist, server eligibility and actual clickable cards are now checked together. Pride remains visible while locked until Order is researched.

Combat automatic acquisition incorrectly skipped zero-attack religious units. Ranged units now acquire them normally, allowing the religious counter bonus to work without manual attack orders.

Large animals previously used navigation cells sized for workers. Their routes now use their own collision clearance, and grove density is reduced. A bounded-travel regression walks a Curelom around a gap that only a worker can fit through.

The viewport test process used to exit successfully even when assertions failed. Its exit status and the runner's JSON verification now both detect failures.

## Evidence and reproduction

Run `Tools/Test-Revision-v05.ps1` for native and viewport checks, and `Tools/Test-LAN-v05.ps1` for multiplayer checks. Test sessions run offscreen on a separate network port and only stop processes that they started.

Reports, screenshots and logs are under `Docs/Evidence/v0.5/`. The model-labeled task checklist is [RevisionTasks-v0.5](RevisionTasks-v0.5.md). `Builds/Chronicles-LAN-Windows.json` records the final portable archive size, SHA-256 digest and CRC verification.
