"""Create and verify a shareable build, excluding local test output and debug symbols."""
from pathlib import Path
import hashlib
import json
import wave
import zipfile

root = Path(__file__).resolve().parents[1]
package = root / "Builds" / "Windows"
output = root / "Builds" / "Chronicles-LAN-Windows.zip"
assert (package / "Chronicles.exe").is_file(), "Package the game first."
manifest = json.loads((package / "Chronicles/Content/Data/visuals.json").read_text(encoding="utf-8"))
catalog = json.loads((package / "Chronicles/Content/Data/catalog.json").read_text(encoding="utf-8"))
image_paths = sorted({item["texture"] for group in ("sprites", "icons")
                      for item in manifest.get(group, {}).values() if item.get("texture")})
assert image_paths, "The art manifest must contain generated assets."
for relative in image_paths:
    assert (package / "Chronicles/Content/Data" / manifest["art_root"] / relative).is_file(), relative
audio_paths = sorted({item["voice_audio"]["select"]
                      for group in catalog.values() if isinstance(group, list)
                      for item in group if isinstance(item.get("voice_audio"), dict)
                      and item["voice_audio"].get("select")})
assert audio_paths, "The catalog must reference at least one voice WAV."
for relative in audio_paths:
    path = package / "Chronicles/Content/Data" / relative
    assert path.is_file() and path.suffix.lower() == ".wav", relative
    with wave.open(str(path), "rb") as voice:
        assert (voice.getcomptype(), voice.getnchannels(), voice.getsampwidth(), voice.getframerate()) == (
            "NONE", 1, 2, 24000), f"Voice WAV must be PCM mono 16-bit 24000 Hz: {relative}"
files = sorted(p for p in package.rglob("*") if p.is_file()
               and "Saved" not in p.relative_to(package).parts
               and p.suffix.lower() != ".pdb"
               and not p.name.startswith("Manifest_"))
with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for path in files:
        archive.write(path, "Chronicles-LAN/" + path.relative_to(package).as_posix())
with zipfile.ZipFile(output) as archive:
    bad = archive.testzip()
    assert bad is None, f"Archive checksum failure: {bad}"
    assert "Chronicles-LAN/Chronicles.exe" in archive.namelist()
    assert "Chronicles-LAN/Chronicles/Content/Data/catalog.json" in archive.namelist()
    for name in ("names.json", "visuals.json"):
        assert "Chronicles-LAN/Chronicles/Content/Data/" + name in archive.namelist()
    for name in ("Controls.md", "Naming.md", "ArtReplacement.md", "ArtGallery.html", "Validation-v0.8.md"):
        assert "Chronicles-LAN/Guides/" + name in archive.namelist()
    for relative in image_paths:
        assert "Chronicles-LAN/Chronicles/Content/Data/" + manifest["art_root"] + "/" + relative in archive.namelist()
    for relative in audio_paths:
        assert "Chronicles-LAN/Chronicles/Content/Data/" + relative in archive.namelist()
    assert "Chronicles-LAN/Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe" in archive.namelist()
report = {"archive": str(output), "files": len(files), "art_images": len(image_paths), "voice_wavs": len(audio_paths), "bytes": output.stat().st_size,
          "sha256": hashlib.file_digest(output.open("rb"), "sha256").hexdigest(),
          "archive_crc_verified": True}
(output.parent / "Chronicles-LAN-Windows.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report, indent=2))
