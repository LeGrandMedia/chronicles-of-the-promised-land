"""Import and validate a v0.9 skeletal FBX bake in the UE5.3 content-only project."""

import json
import os
from pathlib import Path

import unreal


REVISION = Path(__file__).resolve().parents[1]
MANIFEST = REVISION / "Game/Saved/SkeletalBake/skeletal-models.json"
DESTINATION = "/Game/Art/Skeletal"
MATERIAL = "/Game/Art/Materials/M_SkinnedFaceted"
TOLERANCE_CM = 0.05


def fail_unless(condition, message):
    if not condition:
        raise RuntimeError(message)


def import_options():
    options = unreal.FbxImportUI()
    for name, value in {
        "automated_import_should_detect_type": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "override_full_name": True,
        "import_mesh": True,
        "import_as_skeletal": True,
        "import_animations": False,
        "import_materials": False,
        "import_textures": False,
        "create_physics_asset": False,
        "skeleton": None,
    }.items():
        options.set_editor_property(name, value)
    mesh_options = options.get_editor_property("skeletal_mesh_import_data")
    for name, value in {
        "convert_scene": False,
        "convert_scene_unit": False,
        "force_front_x_axis": False,
        "import_translation": unreal.Vector(),
        "import_rotation": unreal.Rotator(),
        "import_uniform_scale": 1.0,
        "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
        "vertex_color_import_option": unreal.VertexColorImportOption.REPLACE,
        "import_morph_targets": False,
        "update_skeleton_reference_pose": True,
        "use_t0_as_ref_pose": False,
    }.items():
        mesh_options.set_editor_property(name, value)
    return options


def pack_cm(point):
    return unreal.Vector(*(axis * 100 for axis in point))


def xyz(vector):
    return [vector.x, vector.y, vector.z]


def verify_geometry(mesh, info):
    label = info["asset"]
    bounds = mesh.get_imported_bounds()
    actual_bounds = [xyz(bounds.origin - bounds.box_extent), xyz(bounds.origin + bounds.box_extent)]
    for actual, expected in zip(actual_bounds, info["bounds_m"]):
        fail_unless(all(abs(a - 100 * b) <= TOLERANCE_CM for a, b in zip(actual, expected)),
                    f"{label}: unexpected imported bounds {actual_bounds}")

    probe = unreal.SkeletalMeshComponent()
    probe.set_skeletal_mesh_asset(mesh)
    rig = info["rig"]
    poses = {}
    fail_unless(probe.get_num_bones() == len(rig), f"{label}: wrong bone count")
    for index in range(probe.get_num_bones()):
        bone = str(probe.get_bone_name(index))
        fail_unless(bone in rig, f"{label}: unexpected bone {bone}")
        parent = rig[bone].get("parent")
        fail_unless(str(probe.get_parent_bone(bone)) == (parent or "None"),
                    f"{label}: unexpected parent for {bone}")
        local = probe.get_ref_pose_transform(index)
        component = unreal.MathLibrary.compose_transforms(local, poses[parent]) if parent else local
        actual = xyz(component.translation)
        expected = rig[bone]["pivot"]
        fail_unless(all(abs(a - 100 * b) <= TOLERANCE_CM for a, b in zip(actual, expected)),
                    f"{label}: pivot for {bone} is {actual}, expected {xyz(pack_cm(expected))}")
        fail_unless(all(abs(scale - 1) <= 0.001 for scale in xyz(component.scale3d)),
                    f"{label}: unexpected bind scale for {bone}")
        poses[bone] = component
    return actual_bounds, poses


def import_mesh(asset_tools, source, info, material):
    task = unreal.AssetImportTask()
    for name, value in {
        "filename": str(source),
        "destination_path": DESTINATION,
        "destination_name": info["asset"],
        "automated": True,
        "replace_existing": True,
        "replace_existing_settings": True,
        "factory": unreal.FbxFactory(),
        "options": import_options(),
        "save": False,
    }.items():
        task.set_editor_property(name, value)
    asset_tools.import_asset_tasks([task])
    meshes = [asset for asset in task.get_objects() if isinstance(asset, unreal.SkeletalMesh)]
    fail_unless(len(meshes) == 1, f"{source}: expected one skeletal mesh, imported {task.get_objects()}")
    mesh = meshes[0]
    label = info["asset"]
    fail_unless(mesh.get_path_name() == f"{DESTINATION}/{label}.{label}", f"{label}: imported to {mesh.get_path_name()}")
    bounds, poses = verify_geometry(mesh, info)

    slots = list(mesh.get_editor_property("materials"))
    fail_unless(len(slots) == 1, f"{label}: expected one material slot, found {len(slots)}")
    slots[0].set_editor_property("material_interface", material)
    mesh.set_editor_property("materials", slots)
    animation = info["animation"]
    for name, socket in animation.get("sockets", {}).items():
        fail_unless(socket["bone"] in poses, f"{label}: socket {name} references an unknown bone")

    library = unreal.EditorAssetLibrary
    library.set_metadata_tag(mesh, "RTSModelSource", info["source"])
    library.set_metadata_tag(mesh, "RTSAnimation", json.dumps(animation, separators=(",", ":")))
    skeleton = mesh.get_editor_property("skeleton")
    fail_unless(skeleton is not None, f"{label}: expected a skeleton asset")
    for asset in (skeleton, mesh):
        fail_unless(library.save_loaded_asset(asset, False), f"Could not save {asset.get_path_name()}")
    unreal.log(f"RTS_SKELETAL_IMPORTED {mesh.get_path_name()} bones={len(poses)} bounds_cm={bounds}")
    return {
        "mesh": mesh.get_path_name(),
        "skeleton": skeleton.get_path_name(),
        "material": material.get_path_name(),
        "bounds_cm": bounds,
        "bones": {bone: xyz(pose.translation) for bone, pose in poses.items()},
    }


def main():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    fail_unless(project == (REVISION / "Tools/CookProject").resolve(), "Use Tools/CookProject/Chronicles.uproject")
    path = Path(os.environ.get("RTS_SKELETAL_BAKE_MANIFEST", MANIFEST)).resolve()
    manifest = json.loads(path.read_text(encoding="utf-8"))
    fail_unless(manifest.get("version") == 1 and manifest.get("models"), f"Invalid bake manifest {path}")
    entries = manifest["models"]
    for entry in entries:
        file_name = f"{entry['asset']}.fbx"
        fail_unless(entry["fbx"] == file_name and Path(file_name).name == file_name and (path.parent / file_name).is_file(),
                    f"Missing or invalid FBX file {entry['fbx']}")
    material_path = os.environ.get("RTS_SKELETAL_BAKE_MATERIAL", MATERIAL)
    material = unreal.EditorAssetLibrary.load_asset(material_path)
    fail_unless(isinstance(material, unreal.MaterialInterface), f"Missing material {material_path}")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    results = [import_mesh(asset_tools, path.parent / entry["fbx"], entry, material) for entry in entries]
    evidence = path.with_name("ue-import-results.json")
    evidence.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"RTS_SKELETAL_IMPORT_COMPLETE {len(results)} meshes, {evidence}")


if __name__ == "__main__":
    main()
