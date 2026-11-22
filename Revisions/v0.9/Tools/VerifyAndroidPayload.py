"""Verify Android data hashes plus exact cooked material and skeletal assets (static only)."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
from zipfile import BadZipFile, ZipFile


CONTENT_ROOT = "Chronicles/Content/"
SKELETAL_PREFIX = CONTENT_ROOT + "Art/Skeletal/"
MATERIAL_PREFIX = CONTENT_ROOT + "Art/Materials/"
EXPECTED_SKELETAL_MESHES = 147
EXPECTED_SKELETONS = 147


def canonical_cooked_path(path: str) -> str:
    """Normalize UnrealPak's ../../../Chronicles paths to an exact package path."""
    value = path.strip().strip('"').replace("\\", "/")
    while value.startswith("./"):
        value = value[2:]
    while value.startswith("../"):
        value = value[3:]
    return value.lstrip("/")


def cooked_uasset_paths(listing: str) -> set[str]:
    """Return exact UnrealPak .uasset paths, avoiding substring matches."""
    found = set()
    for line in listing.splitlines():
        # Accept quoted or whitespace-delimited path tokens, but never test a
        # substring of the whole log line (which could match a longer filename).
        for match in re.finditer(r'"([^"\r\n]+)"|(\S+)', line):
            token = (match.group(1) or match.group(2)).rstrip(",;")
            if token.lower().endswith(".uasset"):
                found.add(canonical_cooked_path(token))
    return found


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", nargs="?", type=Path,
                        default=root / "Builds/Android/Chronicles-Android-arm64-ETC2-v0.9.apk")
    parser.add_argument("--engine", type=Path, default=Path(r"C:\Program Files\Epic Games\UE5\UE_5.3"))
    parser.add_argument("--skeletal-source", type=Path, default=root / "Game/Content/Art/Skeletal")
    parser.add_argument("--evidence", type=Path, default=root / "Docs/Evidence/v0.9/android",
                        help="Directory for this build's verification report")
    args = parser.parse_args()
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    work = root / "Game/Saved/Tests/AndroidPayload"
    work.mkdir(parents=True, exist_ok=True)

    report = {
        "apk": str(args.apk.resolve()),
        "payload_files_expected": 0,
        "payload_files_verified_sha256": 0,
        "mapped_3d_models": 0,
        "missing_files": [],
        "different_from_current_source": [],
        "skeletal_source_dir": str(args.skeletal_source.resolve()),
        "expected_skeletal_meshes": EXPECTED_SKELETAL_MESHES,
        "expected_skeleton_assets": EXPECTED_SKELETONS,
        "source_skeletal_mesh_assets": 0,
        "source_skeleton_assets": 0,
        "cooked_skeletal_mesh_assets": 0,
        "cooked_skeleton_assets": 0,
        "missing_skeletal_mesh_paths": [],
        "missing_skeleton_paths": [],
        "missing_cooked_material_paths": [],
        "missing_cooked_paths": [],
        "cooked_faceted_material_present": False,
        "cooked_skinned_faceted_material_present": False,
        "cooked_pak_count": 0,
        "errors": [],
        "passed": False,
        "scope": "Static archive completeness and hashes plus exact cooked mesh/material paths; no device launch, rendering or LAN verification.",
    }

    try:
        if not args.apk.is_file():
            report["errors"].append(f"APK not found: {args.apk}")
        else:
            with ZipFile(args.apk) as apk:
                apk_names = set(apk.namelist())
                if "lib/arm64-v8a/libUnreal.so" not in apk_names:
                    report["errors"].append("Missing native game library lib/arm64-v8a/libUnreal.so")
                payload_file = "assets/main.obb.png"
                if payload_file not in apk_names:
                    report["errors"].append(f"Missing embedded payload {payload_file}")
                else:
                    try:
                        with ZipFile(io.BytesIO(apk.read(payload_file))) as payload:
                            names = set(payload.namelist())
                            data = root / "Game/Content/Data"
                            verified, missing, changed = 0, [], []
                            for source in sorted(data.rglob("*")):
                                if not source.is_file():
                                    continue
                                name = "Chronicles/Content/Data/" + source.relative_to(data).as_posix()
                                report["payload_files_expected"] += 1
                                if name not in names:
                                    missing.append(name)
                                elif hashlib.sha256(payload.read(name)).digest() != hashlib.sha256(source.read_bytes()).digest():
                                    changed.append(name)
                                else:
                                    verified += 1
                            report["payload_files_verified_sha256"] = verified
                            report["missing_files"] = missing
                            report["different_from_current_source"] = changed

                            manifest_name = "Chronicles/Content/Data/Models/models.json"
                            if manifest_name not in names:
                                report["errors"].append(f"Missing model manifest: {manifest_name}")
                            else:
                                manifest = json.loads(payload.read(manifest_name))
                                models = list(manifest.get("models", {}).values()) + list(manifest.get("variants", {}).values())
                                report["mapped_3d_models"] = len(models)
                                for model in models:
                                    path = "Chronicles/Content/Data/Models/" + model
                                    if path not in names:
                                        report["missing_files"].append(path)

                            source_assets = sorted(args.skeletal_source.glob("*.uasset"))
                            mesh_assets = [asset for asset in source_assets if not asset.stem.endswith("_Skeleton")]
                            skeleton_assets = [asset for asset in source_assets if asset.stem.endswith("_Skeleton")]
                            report["source_skeletal_mesh_assets"] = len(mesh_assets)
                            report["source_skeleton_assets"] = len(skeleton_assets)
                            if len(mesh_assets) != EXPECTED_SKELETAL_MESHES:
                                report["errors"].append(
                                    f"Expected {EXPECTED_SKELETAL_MESHES} staged skeletal meshes, found {len(mesh_assets)} in {args.skeletal_source}"
                                )
                            if len(skeleton_assets) != EXPECTED_SKELETONS:
                                report["errors"].append(
                                    f"Expected {EXPECTED_SKELETONS} staged skeleton assets, found {len(skeleton_assets)} in {args.skeletal_source}"
                                )
                            expected_mesh_paths = {
                                SKELETAL_PREFIX + asset.name for asset in mesh_assets
                            }
                            expected_skeleton_paths = {
                                SKELETAL_PREFIX + asset.name for asset in skeleton_assets
                            }

                            pak_paths = [name for name in names if name.startswith("Chronicles/Content/Paks/") and name.endswith(".pak")]
                            report["cooked_pak_count"] = len(pak_paths)
                            if len(pak_paths) != 1:
                                report["errors"].append(f"Expected exactly one cooked game pak, found {len(pak_paths)}")
                            else:
                                pak = work / "Chronicles-Android-verification.pak"
                                pak.write_bytes(payload.read(pak_paths[0]))
                    except BadZipFile as exc:
                        report["errors"].append(f"Embedded payload is not a valid zip: {exc}")

        if report["cooked_pak_count"] == 1:
            unrealpak = args.engine / "Engine/Binaries/Win64/UnrealPak.exe"
            if not unrealpak.is_file():
                report["errors"].append(f"UnrealPak was not found: {unrealpak}")
            else:
                try:
                    listing = subprocess.run(
                        [str(unrealpak), str(work / "Chronicles-Android-verification.pak"), "-List"],
                        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True,
                        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
                    ).stdout
                    cooked_paths = cooked_uasset_paths(listing)
                    report["missing_skeletal_mesh_paths"] = sorted(expected_mesh_paths - cooked_paths)
                    report["missing_skeleton_paths"] = sorted(expected_skeleton_paths - cooked_paths)
                    report["cooked_skeletal_mesh_assets"] = len(expected_mesh_paths) - len(report["missing_skeletal_mesh_paths"])
                    report["cooked_skeleton_assets"] = len(expected_skeleton_paths) - len(report["missing_skeleton_paths"])
                    report["cooked_faceted_material_present"] = (
                        MATERIAL_PREFIX + "M_Faceted.uasset" in cooked_paths
                    )
                    report["cooked_skinned_faceted_material_present"] = (
                        MATERIAL_PREFIX + "M_SkinnedFaceted.uasset" in cooked_paths
                    )
                    report["missing_cooked_material_paths"] = sorted(
                        path for path in (
                            MATERIAL_PREFIX + "M_Faceted.uasset",
                            MATERIAL_PREFIX + "M_SkinnedFaceted.uasset",
                        ) if path not in cooked_paths
                    )
                    report["missing_cooked_paths"] = sorted(
                        report["missing_skeletal_mesh_paths"]
                        + report["missing_skeleton_paths"]
                        + report["missing_cooked_material_paths"]
                    )
                except (OSError, subprocess.CalledProcessError) as exc:
                    report["errors"].append(f"Could not list cooked pak with UnrealPak: {exc}")

        report["passed"] = (
            report["payload_files_expected"] > 0
            and report["payload_files_verified_sha256"] == report["payload_files_expected"]
            and not report["missing_files"]
            and not report["different_from_current_source"]
            and report["source_skeletal_mesh_assets"] == EXPECTED_SKELETAL_MESHES
            and report["source_skeleton_assets"] == EXPECTED_SKELETONS
            and report["cooked_skeletal_mesh_assets"] == EXPECTED_SKELETAL_MESHES
            and report["cooked_skeleton_assets"] == EXPECTED_SKELETONS
            and report["cooked_faceted_material_present"]
            and report["cooked_skinned_faceted_material_present"]
            and report["cooked_pak_count"] == 1
            and not report["errors"]
        )
    except (OSError, BadZipFile, json.JSONDecodeError, KeyError, ValueError) as exc:
        report["errors"].append(str(exc))

    (evidence / "payload-verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    if not report["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
