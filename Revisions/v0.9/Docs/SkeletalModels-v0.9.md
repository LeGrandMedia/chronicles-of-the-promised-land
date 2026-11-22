# v0.9 skeletal unit asset bake

`Tools/ExportSkeletalModels.py` converts the original v0.9 animated model recipes’ generated JSON into rest-pose FBX files. `Tools/ImportSkeletalModels.py` imports them as Unreal Engine 5.3 skeletal meshes and validates the reference geometry. Recommended and actual task model is GPT-6.2 Sol.

## Export a laborer

Run in PowerShell from `Revisions/v0.9`. The following starts a separate Blender 5.3 background process with factory settings.

```powershell
$blender = 'D:/SteamLibrary/steamapps/common/Blender/blender.exe'
$bake = Join-Path (Get-Location) 'Game/Saved/SkeletalBake/Laborer'
New-Item -ItemType Directory -Path $bake -Force | Out-Null
$exportScript = (Resolve-Path 'Tools/ExportSkeletalModels.py').Path
$blenderArgs = @('--background', '--factory-startup', '--python-exit-code', '1',
    '--python', ('"{0}"' -f $exportScript), '--', '--key', 'industrious_laborer',
    '--output', ('"{0}"' -f $bake))
$export = Start-Process -FilePath $blender -ArgumentList $blenderArgs -WindowStyle Hidden `
    -RedirectStandardOutput "$bake/blender.log" -RedirectStandardError "$bake/blender.stderr.log" -Wait -PassThru
if ($export.ExitCode -ne 0) { throw "Blender export failed. See $bake/blender.stderr.log" }
```

Repeat `--key` for more keys from `Game/Content/Data/Models/models.json`, such as `industrious_laborer:nephites` or `wild_game`. Use `--all-animated` in place of the key options to export all 147 animated models, including the 146 unit FBXs and the stag. Use a separate output folder for each batch. The bake manifest records each FBX’s source JSON path, reference rig, geometry counts and animation metadata.

## Import into the content-only project

The legacy FBX factory and the `PythonScriptPlugin` must be available in the installed UE5.3 editor. `Tools/CookProject/Chronicles.uproject` enables the plugin and runs both scripts without a gameplay editor build. Generate `/Game/Art/Materials/M_SkinnedFaceted` for the skeletal mesh shader usage and the runtime tint parameters.

```powershell
$editor = 'C:/Program Files/Epic Games/UE5/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = (Resolve-Path 'Tools/CookProject/Chronicles.uproject').Path
$materialScript = (Resolve-Path 'Tools/GenerateSkeletalMaterial.py').Path
$importScript = (Resolve-Path 'Tools/ImportSkeletalModels.py').Path
$editorFlags = @('-unattended', '-nop4', '-nullrhi', '-stdout', '-FullStdOutLogOutput', '-UTF8Output')
$materialArgs = @(('"{0}"' -f $project), '-run=pythonscript', ('-script="{0}"' -f $materialScript)) + $editorFlags
$material = Start-Process -FilePath $editor -ArgumentList $materialArgs -WindowStyle Hidden `
    -WorkingDirectory (Split-Path -Parent $editor) `
    -RedirectStandardOutput "$bake/unreal-material.log" -RedirectStandardError "$bake/unreal-material.stderr.log" -Wait -PassThru
if ($material.ExitCode -ne 0) { throw "Unreal material generation failed. See $bake/unreal-material.log" }

$env:RTS_SKELETAL_BAKE_MANIFEST = (Resolve-Path "$bake/skeletal-models.json").Path
$env:RTS_SKELETAL_BAKE_MATERIAL = '/Game/Art/Materials/M_SkinnedFaceted'
$editorArgs = @(('"{0}"' -f $project), '-run=pythonscript', ('-script="{0}"' -f $importScript)) + $editorFlags
$import = Start-Process -FilePath $editor -ArgumentList $editorArgs -WindowStyle Hidden `
    -WorkingDirectory (Split-Path -Parent $editor) `
    -RedirectStandardOutput "$bake/unreal-import.log" -RedirectStandardError "$bake/unreal-import.stderr.log" -Wait -PassThru
if ($import.ExitCode -ne 0) { throw "Unreal import failed. See $bake/unreal-import.log" }
```

The importer saves a skeletal mesh and skeleton per JSON path under `Tools/CookProject/Content/Art/Skeletal`. It verifies the imported bounds, reference pivots, bone parent names and scale, and material slot count. The commandlet writes `ue-import-results.json` alongside the manifest. To stage the content for the runtime, copy the validated `.uasset` pairs and the new material into the matching folders under `Game/Content/Art`. The cooker project includes `/Game/Art` in its always-cook list.

## Coordinate and material contract

- Source positions and pivots use meters with X forward, Y right and Z up. FBX stores `(100x, -100y, 100z)` in centimeters and reverses the triangle order for Blender’s right-handed axes. UE's legacy importer flips Y on read. Scene and unit conversion are disabled, so the resulting UE points and pivots are `(100x, 100y, 100z)`. Unreal retains the reversed index order and the outward normals, giving clockwise front faces.
- Unit mesh object and armature transforms are identity. The legacy importer strips the Blender null object named `Armature`, preserving `root` as the first actual bone. All source bones have their names and parent hierarchy. There are no animation clips or synthetic leaf bones. Use each imported component-space reference transform as the starting point for the runtime's rigid model-space pose delta. In UE's transform multiplication order this is `ReferenceComponent * ModelSpaceDelta`, with translation in centimeters.
- Each point has a single weight of 1. The bake retains flat normals and packs the raw linear palette into vertex RGB. Alpha is 1 for team accents and 0 for all other vertices. `M_SkinnedFaceted` blends into `TeamColor` by vertex alpha times `TeamWeight`. It then blends into the stealth shade by `StealthAmount` and multiplies the final color by `VisibilityTint`. Defaults preserve the original palette. Unreal's FBX importer truncates normalized RGB to 8-bit channels, which can differ from the procedural renderer's rounding by one byte per channel.
- Bow and longbow models add `string_draw` under the weapon joint at the string midpoint. The two midpoint rings follow this bone. The outer rings follow the weapon. The runtime can translate `string_draw` by the desired center offset while keeping the tips in place. The UE5.3 Python API exposes skeletal socket names and parent bones as read-only. The importer validates the source socket bone names and records the animation data in editor metadata. The runtime reads socket positions from the original JSON.
- Asset names are `SK_` followed by the JSON file stem, with `@` mapped to `__`. The base laborer uses `/Game/Art/Skeletal/SK_industrious_laborer.SK_industrious_laborer`. The Nephite variant uses `SK_industrious_laborer__nephites`.

## Validation evidence

The Blender 5.3 background export and binary FBX audit passed for all 147 meshes. The laborer contains 603 control points, 1,030 triangles, 12 bones and 44 team-tint vertices. The full set contains 102,198 points and 172,708 triangles. Its maximum source-to-FBX position and pivot errors were 0.000015 cm and 0.000008 cm. Per-face normals, vertex flags, rigid weights and bone parent links were checked against the source. The evidence is in `fbx-audit.json` under `Game/Saved/SkeletalBake/Laborer`, `AllUnits` and `AnimatedResources`.

Unreal Engine 5.3 imported all 147 meshes and verified 1,753 bind pivots. Its maximum bounds and pivot errors against the source were 0.000014 cm and 0.000008 cm. All 147 mesh and skeleton pairs and `M_SkinnedFaceted` were copied into `Game/Content/Art`. SHA-256 verified the 295 staged files against the cooker project. See `Game/Saved/SkeletalBake/ue-import-summary.json`.

UE re-exported the laborer, Shiz archer and stag from the imported LOD0. These assets retained the source positions, oriented triangles, raw 8-bit RGB, exact team alpha and rigid assignments at every triangle corner. Reference rotations remained identity and the largest normal vector difference was 0.000020. All 9,204 corners also matched Unreal’s clockwise winding convention when checked against their imported normals. See `ue-roundtrip-audit.json` and `ue-winding-audit.json` in `Game/Saved/SkeletalBake/AnimatedResources`.

UE5.3 defaults `r.FreeSkeletalMeshBuffers` to `0`, which keeps CPU copies of skeletal positions, indices, weights and colors available for packaged regression checks. Appearance, active GPU skinning and frame time are measured in the rendered scene tests.
