# Death rendering and batching — v0.9

Fixture and geometry probe, recommended / actual: GPT-6.2 Sol / GPT-6.2 Sol. Renderer integration, recommended / actual: GPT-6.2 Astra / root integration agent. This documentation summary, recommended / actual: GPT-6 Luna / GPT-6 Luna. The pre-optimization baseline uses Windows Development executable SHA-256 `4F7777E9670E221AAF596BBD008E5231E10AC7B5359BCA8E86EAEA512A998DBD`. The paired optimized timing and geometry runs use `FDF0C2372540BFE502820BA990E9E3FCD11AE38AF45C362296BEF6A17A521C57`. All timing and geometry reports passed and their processes exited 0.

## Measured phases

Times are milliseconds, median / P95. Counts are valid phase samples. “Scene” is the prior completed scene update; it includes pose, and pose includes mesh submission. The first-loss interval is a single frame, shown separately because its one sample is not a useful percentile distribution.

| Visible losses | Phase | n | Wall med / P95 | Scene med / P95 |
|---:|---|---:|---:|---:|
| 100 | Live | 163 | 3.93 / 4.64 | 0.48 / 0.63 |
| 100 | First loss, single frame | 1 | 65.85 / 65.85 | 34.49 / 34.49 |
| 100 | Fall | 24 | 29.31 / 31.90 | 22.83 / 24.39 |
| 100 | Collapse | 24 | 30.00 / 30.85 | 22.97 / 24.84 |
| 100 | Settled | 18 | 27.94 / 32.21 | 22.12 / 25.10 |
| 100 | Sink | 41 | 29.68 / 32.04 | 23.05 / 24.15 |
| 100 | Expiry | 1 | 27.65 / 27.65 | 4.42 / 4.42 |
| 100 | Clean | 153 | 3.19 / 3.96 | 0.02 / 0.08 |
| 400 | Live | 100 | 6.29 / 7.46 | 2.15 / 2.66 |
| 400 | First loss, single frame | 1 | 148.36 / 148.36 | 81.17 / 81.17 |
| 400 | Fall | 9 | 59.56 / 72.20 | 56.21 / 59.55 |
| 400 | Collapse | 10 | 68.68 / 79.50 | 56.87 / 63.07 |
| 400 | Settled | 8 | 64.69 / 70.69 | 51.66 / 59.78 |
| 400 | Sink | 18 | 67.61 / 71.88 | 54.96 / 58.66 |
| 400 | Expiry | 1 | 64.10 / 64.10 | 10.67 / 10.67 |
| 400 | Clean | 153 | 3.13 / 4.00 | 0.01 / 0.07 |

First-loss wall timing includes the authoritative kill loop and filtered snapshot; the scene profile starts after the fixture hook and excludes those operations. The separate kill/snapshot measurements were 0.289 / 0.055 ms at 100 losses and 2.961 / 0.068 ms at 400.

## Coverage and limits

- Both standalone Development fixtures rendered synthetic visible losses with simulation ticks disabled; they do not measure transport, AI, or live-match frame rate. The filtered per-view snapshot path was exercised and timed, but this is not a network or adversarial privacy audit.
- The 100-loss case requested 100 unit and 4 building losses: 104 local effects, cap 256, zero evictions. The 400-loss case requested 400 unit and 8 building losses: cap 256, 152 expected local evictions. Representative poses changed and effects drew in both reports.
- Component and renderer-key checks covered cleanup. Rendered component counts were live → first loss → expiry → clean: 107 → 107 → 3 → 3 (100 case), and 411 → 259 → 3 → 3 (400 case). This exercises bounded effect retention and cleanup; it does not establish long-session memory behavior.
- Platform: Windows, UE 5.3.2, Ryzen 5 5600X, GeForce RTX 4070, D3D12, offscreen 1600×900, VSync and frame cap disabled. Detailed run metadata and measurements: [100-loss report](Evidence/v0.9/performance/death-baseline-20261122/100/death-stress-100.json) · [100 run](Evidence/v0.9/performance/death-baseline-20261122/100/run.json) · [400-loss report](Evidence/v0.9/performance/death-baseline-20261122/400/death-stress-400.json) · [400 run](Evidence/v0.9/performance/death-baseline-20261122/400/run.json).
- Visual evidence came from separate capture-only runs, not the timed runs: [100-loss captures](Evidence/v0.9/performance/death-baseline-20261122/100-capture/) and [400-loss captures](Evidence/v0.9/performance/death-baseline-20261122/400-capture/). Each capture report passed and produced live, collapse, and expired screenshots.
- Both D3D12 logs record a handled `SwapChain4` ensure during offscreen startup, also seen in the prior GPU baseline. The fixture reports passed and processes exited 0; these were not error-free launches. See [100 log](Evidence/v0.9/performance/death-baseline-20261122/100/death-stress.log) and [400 log](Evidence/v0.9/performance/death-baseline-20261122/400/death-stress.log).

## Paired optimized comparison

The optimized binary caches the settled fall pose, sinks procedural death components by changing component Z instead of rebuilding vertex positions, and hoists per-part rotations out of per-vertex work. The original baseline measurements above remain the reference. These runs use fixed real-time phase durations, so valid sample counts differ; first-loss and expiry phases each have one sample. Treat the medians as descriptive fixture results, not inferential estimates.

Wall and scene values are median / P95 milliseconds; each cell shows baseline → optimized. `n` gives baseline → optimized valid samples.

| Phase | 100 losses, n | Wall med/P95 | Scene med/P95 | 400 losses, n | Wall med/P95 | Scene med/P95 |
|---|---:|---:|---:|---:|---:|---:|
| Live | 163 → 160 | 3.93/4.64 → 3.99/4.72 | 0.48/0.63 → 0.49/0.59 | 100 → 100 | 6.29/7.46 → 6.30/7.32 | 2.15/2.66 → 2.19/2.77 |
| First loss* | 1 → 1 | 65.85/65.85 → 68.91/68.91 | 34.49/34.49 → 37.66/37.66 | 1 → 1 | 148.36/148.36 → 147.46/147.46 | 81.17/81.17 → 80.91/80.91 |
| Fall | 24 → 25 | 29.31/31.90 → 28.35/30.83 | 22.83/24.39 → 17.29/18.92 | 9 → 9 | 59.56/72.20 → 64.07/75.23 | 56.21/59.55 → 45.32/47.26 |
| Collapse | 24 → 157 | 30.00/30.85 → 4.06/4.80 | 22.97/24.84 → 0.72/0.94 | 10 → 101 | 68.68/79.50 → 5.32/6.31 | 56.87/63.07 → 1.41/1.66 |
| Settled | 18 → 149 | 27.94/32.21 → 3.35/3.90 | 22.12/25.10 → 0.19/0.27 | 8 → 117 | 64.69/70.69 → 4.16/5.09 | 51.66/59.78 → 0.47/0.68 |
| Sink | 41 → 332 | 29.68/32.04 → 3.58/4.26 | 23.05/24.15 → 0.26/0.35 | 18 → 263 | 67.61/71.88 → 4.54/5.37 | 54.96/58.66 → 0.60/0.79 |
| Expiry* | 1 → 1 | 27.65/27.65 → 10.21/10.21 | 4.42/4.42 → 4.58/4.58 | 1 → 1 | 64.10/64.10 → 15.48/15.48 | 10.67/10.67 → 9.93/9.93 |
| Clean | 153 → 154 | 3.19/3.96 → 3.20/3.85 | 0.02/0.08 → 0.02/0.08 | 153 → 147 | 3.13/4.00 → 3.30/4.04 | 0.01/0.07 → 0.02/0.08 |

At 400 losses the settled and sink wall medians fell about 93%; live and first-loss wall times did not materially change, and the falling phase was about 7.6% slower by median wall time. At 100 losses, settled and sink wall medians fell about 88%. The improvement is concentrated in settled deformation and sinking work; it does not establish an across-the-board death or live-match frame-rate gain.

Geometry verification compared the optimized procedural meshes with legacy positions and normals at five phases in each run. All five windows passed for both counts with zero position, vertex-normal and face-normal error. Each window sampled four unit meshes plus a partial building, alongside three completed buildings at 100 losses or four at 400 losses. Totals were 92,745 vertices / 40,925 triangle normals checked for 100 losses and 105,770 / 47,120 for 400; maximum pose-time error was below 0.000001 seconds. Reports: [100 geometry](Evidence/v0.9/performance/death-cached-20261122/100-geometry/death-stress-100-geometry.json) · [400 geometry](Evidence/v0.9/performance/death-cached-20261122/400-geometry/death-stress-400-geometry.json).

Optimized timing reports and run identities: [100 report](Evidence/v0.9/performance/death-cached-20261122/100/death-stress-100.json) · [100 run](Evidence/v0.9/performance/death-cached-20261122/100/run.json) · [400 report](Evidence/v0.9/performance/death-cached-20261122/400/death-stress-400.json) · [400 run](Evidence/v0.9/performance/death-cached-20261122/400/run.json). Capture-only runs are separate from timing samples: [100 capture report and images](Evidence/v0.9/performance/death-cached-20261122/100-capture/) · [400 capture report and images](Evidence/v0.9/performance/death-cached-20261122/400-capture/). Baseline 100/400 collapse images and optimized 400 collapse/expired images were reviewed; other archived captures are linked above without implying a broader visual review.

The earlier death-cached Windows regression package SHA-256 was `288BA1669B769054AA822E8326D42551ECDAAB068D7DCAE27857B12A83D07F8D` ([historical artifact ledger](Evidence/v0.9/platform/death-cached-artifacts.json)). It passed 60 scene and 67 input checks and differed from measured `FDF0C237...` only by moving death PoseElapsed metadata after the offscreen return. It predates the current map and selection source work and is not the current package identity. It did not rerun the earlier gate, native or LAN suites.


## Opt-in batched-death A/B experiment

The bounded unit-death batching path is implemented behind the explicit `-RTSBatchUnitDeaths` command-line flag. It is off by default and is not enabled in the earlier installer or Android artifacts. Two 400-loss control/trial pairs used the same Windows D3D12 fixture setup, with the authoritative kill loop and filtered snapshot included in the single-sample first-loss wall interval. Both arms retained 256 logical effects and the same 152 expected FIFO evictions; batching reduced physical death-effect components from 256 to 21.

| 400 visible unit losses + 8 buildings | Control, batching off | Trial, batching on |
|---|---:|---:|
| Pair 1 physical components | 256 | 21 |
| Pair 1 first-loss wall | 147.64 ms | 113.11 ms |
| Pair 2 physical components | 256 | 21 |
| Pair 2 first-loss wall | 134.35 ms | 110.74 ms |

The first-loss reduction is encouraging but comes from two single-frame observations. Falling-phase median wall times did not improve consistently: control medians were 66.48 and 54.43 ms; batched medians were 68.67 and 59.06 ms. Do not claim a general fall, live-match, AI, transport, or device performance gain from this experiment.

Expanded correctness evidence passed geometry comparison at 100 and 400 losses and a 400-loss lifecycle run. The geometry runs checked five windows against legacy pose output; the lifecycle run covered three overlapping bursts, 152 FIFO evictions, hidden state, camera reentry, reduced-motion changes, mixed-age sinking and expiry. Evidence: [control 1](Evidence/v0.9/performance/death-batch-ab-control-1/400/death-stress-400.json) · [trial 1](Evidence/v0.9/performance/death-batch-ab-trial-1/400/death-stress-400.json) · [trial 2](Evidence/v0.9/performance/death-batch-ab-trial-2/400/death-stress-400.json) · [control 2](Evidence/v0.9/performance/death-batch-ab-control-2/400/death-stress-400.json) · [100-loss geometry](Evidence/v0.9/performance/death-batch-expanded/100-geometry/death-stress-100-geometry.json) · [400-loss geometry](Evidence/v0.9/performance/death-batch-expanded/400-geometry/death-stress-400-geometry.json) · [400-loss lifecycle](Evidence/v0.9/performance/death-batch-expanded/400-lifecycle/death-stress-400-lifecycle.json).

These standalone Development fixtures pause the simulation tick and exercise rendered authority kills and filtered snapshots. The batching switch remains opt-in; the evidence does not support enabling it by default or treating it as a release performance pass.
