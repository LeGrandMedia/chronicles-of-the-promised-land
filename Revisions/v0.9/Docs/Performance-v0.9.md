# v0.9 rendered performance

Recommended and actual model — GPT-6.2 Sol.

## Scope and method

This probe exercises the packaged 3D client with the regular game HUD, camera, lighting, terrain and animated meshes. Six fixtures use 200 moving units, 30 structures, and 24 resource sites each. The seventh uses 800 Nephite units with the same structure and resource counts.

The fixtures use catalog models and a deterministic formation that moves from synthetic snapshots at 10 Hz. The authoritative world tick is disabled. These measurements cover client presentation and picking. Gameplay and LAN tests supply separate evidence about simulation and networking.

Each scene warms up for at least two seconds and 45 rendered frames before recording 180 consecutive wall-clock frame intervals. This includes the normal game, render, and presentation work while isolating startup, model loads, screenshots, and the dedicated pick workload. VSync, Unreal frame-rate smoothing, and the explicit FPS cap are disabled for the probe.

The pick phase first times one real physical-pixel query per rendered frame. It then repeats verified entity clicks, context clicks, and empty terrain clicks against one displayed pose. Each query uses the production hit test. These timings measure CPU lookup with the bounds cache in its observed state. Triangle counts represent the full geometry of visible components intersecting the viewport before GPU occlusion.

## Reproduction

Run the packaged Windows Development executable with:

```text
-RTSV09RenderStress -RenderOffscreen -windowed -ForceRes -ResX=1600 -ResY=900 -unattended -nosound
```

The current default uses GPU skinning for supported animated models. Add `-RTSProceduralMeshes` to run the same fixtures with procedural geometry.

The run writes `Saved/Tests/v09-render-stress.json` and seven PNG captures below `Saved/Screenshots/RenderStress-*`. The report includes raw steady frame samples, median, 95th percentile, maximum, hardware, render settings, entity counts, and separate pick distributions.

To isolate a fixture, add `-RTSRenderStressCase=N`. The indices are 0 Nephites, 1 Lamanites, 2 Gadianton, 3 Coriantumr, 4 Shiz, 5 Laban, and 6 the 800-unit Nephite fixture. The diagnostic build records the UE game, render, RHI and GPU stage counters alongside completed scene update, pose, mesh submission, bounds, and hit-test costs. The engine counters are asynchronous. Scene update contains pose, pose contains mesh submission, and hit tests contain their own bounds work.

## Results

The uncontended run completed all seven scenes on Windows with Unreal Engine 5.3.2, the default D3D12 renderer, an RTX 4070, a Ryzen 5 5600X, and 127.9 GiB RAM. The viewport was 1600×900 rendered offscreen. VSync and the explicit FPS cap both read back as 0, frame-rate smoothing was disabled, and shadow quality was 3.

| Fixture | Moving units | Entity triangles | Median frame | P95 frame | Max frame | Median verified pick |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Nephites | 200 | 253,192 | 64.62 ms | 69.20 ms | 72.01 ms | 0.051 ms |
| Lamanites | 200 | 288,569 | 76.64 ms | 81.06 ms | 84.02 ms | 0.054 ms |
| Gadianton | 200 | 246,513 | 64.61 ms | 68.83 ms | 71.28 ms | 0.052 ms |
| Coriantumr | 200 | 254,938 | 66.69 ms | 70.75 ms | 73.60 ms | 0.053 ms |
| Shiz | 200 | 247,794 | 64.76 ms | 68.79 ms | 71.03 ms | 0.050 ms |
| Laban | 200 | 280,202 | 70.54 ms | 75.24 ms | 84.08 ms | 0.051 ms |
| Nephites, extreme | 800 | 902,142 | 249.59 ms | 268.68 ms | 326.16 ms | 0.124 ms |

Every fixture retained all expected entities in view and drew 16,384 terrain triangles. The 200-unit scenes ran at about 13–15.5 FPS by median frame time. The 800-unit scene ran at about 4 FPS. Natural first-pick medians ranged from 0.070 to 0.087 ms in the 200-unit scenes and were 0.180 ms in the 800-unit scene. All repeated verified clicks returned the expected result.

The first attempt stalled while selecting a blocked pick candidate. The successful run pinned the fixture camera and advanced past blocked candidates. [Initial attempt](Evidence/v0.9/performance/v09-render-stress.json), [raw final report](Evidence/v0.9/performance/steady/v09-render-stress.json), [final run log](Evidence/v0.9/performance/steady/render-stress.log)

Rendered captures — [Nephites](Evidence/v0.9/performance/steady/nephites-200.png), [Lamanites](Evidence/v0.9/performance/steady/lamanites-200.png), [Gadianton](Evidence/v0.9/performance/steady/gadianton-200.png), [Coriantumr](Evidence/v0.9/performance/steady/coriantumr-200.png), [Shiz](Evidence/v0.9/performance/steady/shiz-200.png), [Laban](Evidence/v0.9/performance/steady/laban-200.png), [800-unit Nephites](Evidence/v0.9/performance/steady/nephites-800.png).

## Timing diagnosis

The instrumented 200-unit Nephite rerun reproduced the baseline with a 64.85 ms median wall frame and a 69.46 ms 95th percentile. Every one of its 180 steady frames recorded 206 pose updates and 660,420 posed vertices.

| Counter | Median | P95 |
| --- | ---: | ---: |
| Unreal game thread | 51.93 ms | 55.17 ms |
| Unreal render thread | 64.32 ms | 68.78 ms |
| Unreal RHI thread | 4.22 ms | 5.37 ms |
| Unreal GPU busy time | 19.63 ms | 23.40 ms |
| Scene update | 45.67 ms | 47.89 ms |
| Pose, including mesh submission | 44.59 ms | 46.88 ms |
| Mesh submission | 21.37 ms | 22.54 ms |
| Bounds | 2.25 ms | 2.67 ms |

The CPU-side scene spent most of its update time posing meshes. Mesh submission accounted for roughly half of the measured pose time. The GPU busy interval and click durations were much smaller than the wall frame. Frame-stage observations can refer to different pipelined frames. Hit timings are reported in the separate pick phase. [Diagnostic report](Evidence/v0.9/performance/diagnostic-nephites-200/v09-render-stress.json), [diagnostic log](Evidence/v0.9/performance/diagnostic-nephites-200/render-stress.log)

## Shared mesh corners and cached pose data

The optimized build reuses coplanar mesh corners and caches rigid normals and static vertex colors. A repeat of the same 200-unit Nephite fixture retained 200 units, 30 structures, 24 resource sites, 253,192 entity triangles, and 16,384 terrain triangles. Both runs passed their picking checks. The captures show the same formation and terrain at different points in the marching cycle. [Baseline capture](Evidence/v0.9/performance/diagnostic-nephites-200/nephites-200.png), [optimized capture](Evidence/v0.9/performance/optimized-corners-200/nephites-200.png)

| Counter | Baseline | Optimized | Change |
| --- | ---: | ---: | ---: |
| Median wall frame | 64.85 ms | 53.43 ms | −17.6% |
| P95 wall frame | 69.46 ms | 56.36 ms | −18.9% |
| Max wall frame | 72.21 ms | 60.42 ms | −16.3% |
| Median game thread | 51.93 ms | 40.91 ms | −21.2% |
| Median render thread | 64.32 ms | 52.94 ms | −17.7% |
| Median scene update | 45.67 ms | 34.58 ms | −24.3% |
| Median pose | 44.59 ms | 33.68 ms | −24.5% |
| Median mesh submission | 21.37 ms | 13.93 ms | −34.8% |
| Median bounds | 2.25 ms | 1.75 ms | −22.2% |
| Pose updates per frame | 206 | 206 | 0% |
| Posed vertices per frame | 660,420 | 528,254 | −20.0% |

The optimized case reached about 18.7 FPS by median frame time. This repeat covers the 200-unit Nephite fixture on the same Windows D3D12 configuration. [Raw optimized report](Evidence/v0.9/performance/optimized-corners-200/v09-render-stress.json), [run log](Evidence/v0.9/performance/optimized-corners-200/render-stress.log)

## GPU-skinned client rendering

The earlier opt-in Windows run passed all 60 scene checks across the six factions. Each faction submitted and drew eight live skin rigs. The loaded mesh library covered all 259 entries. The probe matched actual LOD0 positions skinned with the active component’s weights and matrices against authored pose geometry, along with vertex colors and material parameters. The largest rigid-position difference was 0.00006 cm. Imported front-face checks found zero mismatches. Camera projection error was at most 0.0005 pixels after the probe camera and capture timing were stabilized. [Scene report](Evidence/v0.9/rendering/gpu-settled/scene3d-tests.json), [Nephite capture](Evidence/v0.9/rendering/gpu-settled/scene3d-gpu-nephites.png), [Laban capture](Evidence/v0.9/rendering/gpu-settled/scene3d-gpu-laban.png)

The same seven workload fixtures then passed on the RTX 4070 at 1600×900, uncapped and rendered offscreen. Six animated wildlife sites account for the difference between each unit count and its GPU rig count. The entity triangle totals matched the original Windows run.

| Fixture | Moving units | GPU rigs | GPU buffer vertices | Median frame | P95 frame | Max frame |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Nephites | 200 | 206 | 530,548 | 4.49 ms | 9.22 ms | 10.85 ms |
| Lamanites | 200 | 206 | 625,472 | 4.34 ms | 9.52 ms | 10.44 ms |
| Gadianton | 200 | 206 | 530,490 | 4.47 ms | 9.00 ms | 11.08 ms |
| Coriantumr | 200 | 206 | 541,658 | 4.50 ms | 9.56 ms | 11.51 ms |
| Shiz | 200 | 206 | 528,658 | 4.60 ms | 9.94 ms | 11.31 ms |
| Laban | 200 | 206 | 580,708 | 4.32 ms | 9.70 ms | 12.06 ms |
| Nephites, extreme | 800 | 806 | 2,090,623 | 8.91 ms | 29.14 ms | 34.37 ms |

The 200-unit Nephite fixture also passed a separate repeat at 4.42 ms median and 8.82 ms P95. Across the full run, median first-pick times were 0.138–0.174 ms at 200 units and 0.353 ms at 800. All verified clicks returned the expected target. These are Windows presentation fixtures on this machine. Frame rates during live simulation and on other devices require separate measurements. [Full raw report](Evidence/v0.9/performance/gpu-all/v09-render-stress.json), [single repeat](Evidence/v0.9/performance/gpu-nephites-200/v09-render-stress.json), [800-unit capture](Evidence/v0.9/performance/gpu-all/nephites-800.png)

## Default GPU, staggered poses and ground shadows

The Windows package with GPU skinning enabled by default, staggered 24 Hz poses, and extended sun shadows passed all 60 scene checks and 67 desktop viewport checks. The scene captures show the new shadows on the terrain. [Default GPU scene](Evidence/v0.9/rendering/gpu-default-shadow/scene3d-tests.json), [shadow capture](Evidence/v0.9/rendering/gpu-default-shadow/scene3d-gpu-nephites.png), [desktop regressions](Evidence/v0.9/input/gpu-default-1600x900/input-tests.json)

| Nephite fixture | Prior GPU median / P95 / max | Default GPU median / P95 / max | P95 reduction |
| --- | ---: | ---: | ---: |
| 200 units | 4.49 / 9.22 / 10.85 ms | 5.15 / 5.88 / 6.70 ms | 36.3% |
| 800 units | 8.91 / 29.14 / 34.37 ms | 14.21 / 17.31 / 20.01 ms | 40.6% |

The new build reduced P95 pose updates per frame from 206 to 29 for 200 units and from 806 to 337 for 800 units. Both fixtures retained their visible geometry and actual GPU rig counts and passed all verified picks. The paired changes increased median frame time and reduced the longest frames on this configuration. [Default 200-unit report](Evidence/v0.9/performance/gpu-default-shadow-200/v09-render-stress.json), [default 800-unit report](Evidence/v0.9/performance/gpu-default-shadow-800/v09-render-stress.json)

## Simultaneous losses

Probe implementation, recommended and actual model GPT-6.2 Sol. Death effects use procedural meshes. The standalone probe measures their transition from live meshes, deformation and cleanup. Packaged Windows timing and geometry runs are now recorded below and in the detailed [death performance report](DeathPerformance-v0.9.md).

Run each workload in a fresh Windows Development process. Use `-RTSDeathStressUnits=400` for the larger case.

```text
-RTSV09DeathStress -RTSDeathStressUnits=100 -RenderOffscreen -windowed -ForceRes -ResX=1600 -ResY=900 -unattended -nosound
```

The 100-unit case destroys four buildings and expects 104 effects. The 400-unit case destroys eight buildings and expects 256 effects, the existing client cap. Deterministic buildings-last ordering leaves 248 unit effects and all eight building effects after the oldest 152 unit events are evicted. Both cases use authority `World.Kill` and real filtered snapshots with simulation tick paused. A separately scouted enemy unit and remembered building test fog privacy. They are outside the visible casualty counts.

Reports go to `Saved/Tests/death-stress-100.json` and `Saved/Tests/death-stress-400.json`. They include raw frame samples, sample counts and distributions for the warm live scene, first loss, fall, collapse, settled poses, sink, expiry and clean scene. First-loss wall time includes authority kills and snapshot delivery. The scene timing begins after both and reports component registration, initial mesh submission, pose work and retirement. Unreal game, render, RHI and GPU counters can refer to different pipelined frames. These scopes overlap. Process resident memory at named checkpoints provides context for separately verified component registration and cleanup.

For screenshots, repeat each case with `-RTSDeathStressCapture`. The capture run writes a separate `death-stress-<units>-capture.json` and live, collapse and expired PNGs under `Saved/Screenshots/death-stress-<units>-<UTC>/`. The timing runs issue no screenshot requests. The probe verifies visible changing poses, exact FIFO retention, preserved fog memory, removal from selection and exact picking, and effect-component expiry. Remote batch transport and full gameplay simulation have separate tests.

### Cached death animation comparison

The paired optimized run uses Windows Development binary SHA-256 `FDF0C2372540BFE502820BA990E9E3FCD11AE38AF45C362296BEF6A17A521C57`, compared with the baseline in [DeathPerformance-v0.9](DeathPerformance-v0.9.md). It caches settled poses, moves sinking components by Z, and hoists per-part rotations. At 400 losses, settled and sink median wall costs fell about 93%; the first-loss frame was essentially unchanged (148.36 → 147.46 ms, one sample), and fall wall time was slightly worse (59.56 → 64.07 ms). Five geometry windows at each workload matched legacy positions and normals with zero reported error, including partially built buildings. Phase sample counts differ because phases run for fixed real durations; these fixture results do not establish an overall death-rendering or live-match frame-rate improvement.

Use `Tools/Test-DeathStress-v09.ps1 -EvidenceName <new-name>` to run and archive both workloads with executable/source hashes. Add `-VerifyGeometry` or `-Capture` in separate runs; the runner refuses to overwrite an existing case directory.

### Opt-in batched-death experiment

The bounded unit-death batching path is implemented behind `-RTSBatchUnitDeaths`; it remains disabled by default and is absent from the earlier installer and Android artifacts. Two 400-loss control/trial pairs kept 256 logical effects and 152 FIFO evictions while reducing physical death components from 256 to 21. The single-sample first-loss wall intervals were 134–148 ms with batching off and 111–113 ms with it on. Falling-phase wall medians moved in both directions across the two pairs, so the trial does not show a consistent fall-phase gain. See the detailed [death performance report](DeathPerformance-v0.9.md) and [four A/B reports](Evidence/v0.9/performance/).

Expanded geometry checks passed at 100 and 400 losses, and a 400-loss lifecycle probe passed across three overlapping bursts, eviction, hidden state, camera reentry, reduced motion and expiry. These were standalone Windows client fixtures with simulation paused; they do not establish live-match, transport, AI, or mobile performance. Keep batching opt-in pending broader measurements.
