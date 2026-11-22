# GPU-skinned rendering status — v0.9

Best-suited model: GPT-6.2 Astra for Unreal rendering integration; GPT-6.2 Sol for asset baking and verification. Actual contributors: Root and GPT-6.2 Sol agents. This is implementation and test status, not a claim that every platform or review pass is complete.

## Current implementation

GPU skeletal rendering is the default for the 147 animated assets in the 259-model library. Poses run at staggered 24 Hz, sun shadows extend to 45,000 cm, and procedural meshes plus original JSON remain available as fallback. The path preserves authoritative simulation and filtered snapshots. Current executable/artifact hashes and any outstanding regression are tracked once in [Development-v0.9](Development-v0.9.md); an earlier test pass is not a release declaration.

Seven building assets were subsequently refined for civic scale and Laban silhouettes, passing the [60-check building scene](Evidence/v0.9/rendering/building-readability/scene3d-tests.json). Queued walls now form mitered corners from their actual models; their [102-check probe](Evidence/v0.9/rendering/wall-miter-first/wall-junction-tests.json) covers visible geometry, selection, construction, gates and filtered neighbors. See the [wall contract](WallJunctionPlan-v0.9.md) for asset-axis requirements and remaining gate-clearance work.

## Verified earlier results

- The default GPU scene passed 60/60 and desktop input 67/67. Metadata/test-mode isolation then passed the [full native replay](Evidence/v0.9/native/sprite-mode-fixed/SelfTest-v09-sprite-mode-fixed.log), including 119 presentation assertions and visual PNG/GC/hitbox checks. Raw earlier fixture failures remain preserved.
- Earlier full-library tests loaded all 259 models, with six faction/camera geometry, picking and projection checks. Actual skinned vertices were matched against authored poses; largest reported rigid-position difference was 0.00006 cm and projection error was below 0.001 pixels. See [settled GPU scene tests](Evidence/v0.9/rendering/gpu-settled/scene3d-tests.json) and [combat-presentation tests](Evidence/v0.9/rendering/combat-presentation/scene3d-tests.json).
- The default-GPU/extended-shadow/staggered-pose build passed 60 scene checks and 67 desktop checks before the latest metadata/test-mode change. The earlier measured synthetic client fixtures reported median / P95 of 5.15 / 5.88 ms at 200 Nephite units and 14.21 / 17.31 ms at 800. Fixture setup, hardware, limits and raw captures are in [Performance-v0.9](Performance-v0.9.md).

These are Windows client-presentation fixtures on the development PC (RTX 4070, UE 5.3.2, D3D12, 1600×900, offscreen). The workload advances synthetic snapshots with authoritative world ticking disabled. It does not measure live-match frame rate, other GPUs, Mac, Android, iPhone, or player perception. The good median does not erase the P95/max values or establish a device performance target.

## Reproducible checks and remaining work

When a later change touches the renderer, poses, or content, use the Windows Development package after Root confirms no build is replacing it:

```text
-RTSV09SceneTest -RenderOffscreen -d3d11 -ResX=1600 -ResY=900 -ForceRes -windowed -unattended -nosound
```

Run desktop input separately with `-RTSInputTest`; use `-RTSV09WallJunctionTest` for wall changes. Preserve fresh logs, JSON reports and captures, then review the images. Run graphical probes sequentially: concurrent offscreen startup has produced a D3D12 swap-chain ensure. Repeat only checks relevant to changed geometry, poses, picking or content; performance methodology is in the dedicated report.

Android payload verification confirms the 147 skeletal meshes/skeletons and both materials in the APK; physical rendering, touch and mixed-platform LAN remain unverified. Refresh platform packages after Windows corrections are verified. macOS/iOS have no artifact until compatible Mac/Xcode/signing access exists. Continue district-overlap, gate art/clearance, other-GPU, balance and external-AI review.

## Evidence references

- [Current default-shadow scene report (prior package)](Evidence/v0.9/rendering/gpu-default-shadow/scene3d-tests.json)
- [200-unit synthetic fixture (package 4128)](Evidence/v0.9/performance/gpu-default-shadow-200/v09-render-stress.json)
- [800-unit synthetic fixture (package 4128)](Evidence/v0.9/performance/gpu-default-shadow-800/v09-render-stress.json)
- [Desktop input report (prior package)](Evidence/v0.9/input/gpu-default-1600x900/input-tests.json)
- [Performance method, full results and hardware limits](Performance-v0.9.md)
- [Model library and runtime format](Models-v0.9.md)
- [Android package status](Android-v0.9.md)
