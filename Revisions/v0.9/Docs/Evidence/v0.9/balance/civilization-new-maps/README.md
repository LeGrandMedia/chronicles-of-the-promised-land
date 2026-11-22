# Interrupted second-pass civilization trial

This directory contains partial evidence from the second-pass balance trial on the newer maps. The trial tested reduced Nephite recruitment premiums. Root stopped the first map test after the separate Zarahemla comparison rejected that pricing direction, so this is an interrupted exploratory run, not a completed balance suite or final map-balance proof.

The complete six-game Zarahemla trial in `civilization-second-tuned` produced 0 Nephite wins, 5 losses, and 1 timeout; both Nephite Seats were lost in that timeout. Its complete six-game control in `civilization-second-control` produced 0 wins, 4 losses, and 2 genuine timeouts. Those counts do not describe this interrupted new-map directory. The small deterministic samples do not establish general human faction strength.

The normal Nephite premiums have been restored to 1.25 and 1.20. Current source retains the improved Nephite HP tuning, the Lamanite 75% cost/training time, and the Gadianton 45% manual-gather rate. Reduced recruitment pricing is available only through the experimental `-RTSV09BotSecondPassRecruitment` path; it is not the normal bot behavior. The DE3E20 executable and its installer/APK are historical trial artifacts for this pricing test, not current candidate packages. The code changes for maps, UI, touch, and LAN in that executable were unchanged by the pricing rollback, but its balance outcomes are not evidence for the restored production tuning.

The final test suite and rebuilt artifact identities will be recorded in the current balance report and platform ledger when available. Do not treat this directory's partial results as a finished second-pass balance result.
