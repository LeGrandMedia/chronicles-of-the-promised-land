# v0.3 revision validation

The packaged build at `D:\book of mormon RTS\Revisions\v0.3\Builds\Windows` was built and checked on 2026-10-21. The original v0.2 launcher and package remain separate.

## Results

- Win64 Development build: passed, Unreal Engine 5.3.
- Native regression suites: 167 checks passed (26 core, 17 scenarios, 43 customization, 6 art, 6 rally, 26 economy/pathing/fog, 43 religion/hero progression).
- Actual game viewport input: 22 checks passed at 1600×900 and again at 1280×720. This covers single click, resource right click and delivery, drag selection, idle workers, build cards and configured hotkeys, context-appropriate commands, rally, rotation and inverse mapping, Shift control-group append, idle military, age-choice filtering, and exact queue-item cancellation.
- Ten clustered workers: all ten gathered and delivered through normal simulated movement. No fixture teleports into gathering slots.
- Fog memory: seen structures retain their last known minimap information through unseen damage and destruction; scouting the empty site removes the remembered icon.
- Four independent multiplayer processes: passed on localhost port 17888, with 200 units per player and over 800 visible entities. All clients verified movement, queued waypoints, training, construction, rally replication, Liahona failure/restoration, and the final winner.
- Compressed network state: largest measured raw client snapshot 134,280 bytes; largest measured compressed snapshot 5,643 bytes. The earlier reliable-buffer overflow is resolved in this stress run.
- Original copy isolation: all 56 protected original source/configuration/content/launcher/package files retain their recorded SHA-256 hashes. Test windows were offscreen; only revision test processes were started or stopped.

Reports and screenshots are in `Docs/Evidence/v0.3`. The network test used four processes on this computer; a match across separate physical LAN computers was not performed. Balance and play feel still benefit from your playtesting. Note 46 was incomplete and is covered by the neighboring priest and ability changes.

## Run this revision

```powershell
cd "D:\book of mormon RTS\Revisions\v0.3"
.\Play.cmd
```

All LAN players should use this same v0.3 package. The original workspace-root `Play.cmd` continues to launch v0.2.
