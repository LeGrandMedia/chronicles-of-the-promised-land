# Replacing artwork and audio

The game reads `Game/Content/Data/visuals.json` and PNGs under `Game/Content/Data/Art/` at startup. In a packaged build, use `Builds/Windows/Chronicles/Content/Data/`. Restart the game after editing; cosmetic files are local to each player.

Use stable catalog IDs, not display names. Keep paths relative to the manifest's `art_root` (normally `Art`) and inside that folder. Replace an existing manifest texture when you want the change to affect every use of that image; do not add another entry for an older version. The [Art Gallery](ArtGallery.html) lists the unique image files referenced by the current manifest.

## Sprites and icons

`visuals.json` separates map `sprites` from UI `icons`. A sprite can define its PNG `texture`, maximum logical `size`, ground `pivot`, optional `hitbox` and `context_hitbox`, color, and animations. A faction variant uses `<stable-id>@<faction-id>` and takes precedence for that faction. Icons use independent IDs and may refer to the same PNG as a sprite. Keep the shipped catalog IDs and manifest structure intact; missing or invalid art falls back to procedural drawing.

`size` is the maximum box in logical pixels at 1600×900 and zoom 1; `pivot` is the normalized ground anchor. Hitbox bounds use normalized `min`/`max` pairs. Icon and frame `uv` values are normalized `[x, y, width, height]`. The loader caches sprites at up to 512px and icons at 256px; no image decoding occurs during drawing.

PNG sources keep their aspect ratio and alpha. The loader may downsize large sprites and icons for runtime, but does not alter the source files. Transparent margins and frame dimensions should be consistent so the pivot and selection bounds stay aligned.

## Animation frames

Animation entries accept `fps`, `loop`, and `frames`; frame paths are relative to `art_root`. Existing states cover idle, movement, attacks, work, gathering, building, and conversion. Unit sprites also accept `death`, and building sprites accept `destroy`. Set these transient states to `"loop": false`; the runtime holds the last frame for the brief event. Death and destruction are short, approximately 3.2-second visibility events delivered reliably to clients that saw the event. They are not persistent world states.

```json
"death": {
  "fps": 8,
  "loop": false,
  "frames": ["units/example-death-1.png", "units/example-death-2.png"]
}
```

Without explicit frames, units use the runtime's 4-by-6 weighted UV mesh for 2D puppet motion. The mesh deforms the source image; it does not add hidden surfaces or 3D rotation. Explicit animation frames replace that procedural pose for their state. Keep animation artwork in a consistent view and use additional frames where a pose needs a distinct silhouette.

The v0.8 art set adds eight Laban building images: the Winery and faction-specific Great and Spacious Building, headquarters, homes, armory, market, storehouse, and guard camp. Update the existing Laban entries in `visuals.json` to replace these images; the gallery follows the manifest paths.

## Voice lines

Voice text belongs in the catalog entry as `"voiceLines": {"select": "Your line"}`; `select` is a string. An optional `voice_audio.select` value names a WAV path relative to `Game/Content/Data`, for example `Audio/gadianton_precious_things.wav`. Use PCM WAV, mono, 16-bit, 24,000 Hz. The Gadianton Robber's “I loooooove precious things” line has one placeholder WAV made with offline TTS; it is not a recorded performance. Missing voice audio leaves the selection subtitle available.
