"""Export v0.9 animated models as rigidly weighted FBX files with Blender in background mode."""

import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import sys

import bpy


REVISION = Path(__file__).resolve().parents[1]
MODEL_ROOT = REVISION / "Game/Content/Data/Models"
BAKE_ROOT = REVISION / "Game/Saved/SkeletalBake"
DEFAULT_KEY = "industrious_laborer"
BOW_EQUIPMENT = {"bow", "longbow"}


def fbx_point(point):
    x, y, z = point
    return (x * 100, -y * 100, z * 100)


def read_model(source):
    return json.loads((MODEL_ROOT / source).read_text(encoding="utf-8"))


def model_paths(keys, all_units, all_animated):
    manifest = json.loads((MODEL_ROOT / "models.json").read_text(encoding="utf-8"))
    catalog = manifest["models"] | manifest["variants"]
    if all_units:
        return sorted({path for path in catalog.values() if path.startswith("units/")})
    if all_animated:
        return [path for path in sorted(set(catalog.values())) if read_model(path).get("animation", {}).get("style")]
    paths = set()
    for key in keys or [DEFAULT_KEY]:
        path = catalog.get(key)
        if not path or not read_model(path).get("animation", {}).get("style"):
            raise ValueError(f"{key!r} is not an animated model key in models.json")
        paths.add(path)
    return sorted(paths)


def get_rig(model):
    rig = dict(model["rig"])
    animation = model.get("animation", {})
    if animation.get("equipment") in BOW_EQUIPMENT:
        sockets = animation["sockets"]
        lower = sockets["bowstring_lower"]
        upper = sockets["bowstring_upper"]
        if lower["bone"] != upper["bone"]:
            raise ValueError("Bowstring sockets must be on the same bone")
        midpoint = [(a + b) / 2 for a, b in zip(lower["position"], upper["position"])]
        rig["string_draw"] = {"pivot": midpoint, "parent": lower["bone"]}
    return rig


def make_armature(rig):
    scene = bpy.context.scene
    data = bpy.data.armatures.new("RTSRig")
    # UE's legacy FBX importer strips this Blender null node, leaving the JSON root.
    armature = bpy.data.objects.new("Armature", data)
    scene.collection.objects.link(armature)
    bpy.context.view_layer.objects.active = armature
    armature.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for name, entry in rig.items():
        bone = data.edit_bones.new(name)
        x, y, z = fbx_point(entry["pivot"])
        bone.head = (x, y, z)
        bone.tail = (x, y + 5, z)
        bone.use_connect = False
    for name, entry in rig.items():
        if "parent" in entry:
            data.edit_bones[name].parent = data.edit_bones[entry["parent"]]
    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


def make_mesh(name, model, rig, armature):
    positions = []
    triangles = []
    colors = []
    bone_vertices = defaultdict(list)
    for part in model["parts"]:
        base = len(positions)
        source = part["vertices"]
        positions.extend(map(fbx_point, source))
        indices = part["triangles"]
        # Reflection from model to Blender axes reverses the winding.
        for first in range(0, len(indices), 3):
            a, b, c = indices[first:first + 3]
            triangles.append((base + a, base + c, base + b))
        team = part.get("team", False) or "team" in part["name"]
        colors.extend([(*part["color"][:3], float(team))] * len(source))

        string_segment = "string_draw" in rig and part["name"] in {"bowstring_lower", "bowstring_upper"}
        if string_segment:
            draw = rig["string_draw"]["pivot"]
            tip = model["animation"]["sockets"][part["name"]]["position"]
        for index, point in enumerate(source, base):
            bone = part["bone"]
            if string_segment and math.dist(point, draw) < math.dist(point, tip):
                bone = "string_draw"
            bone_vertices[bone].append(index)

    data = bpy.data.meshes.new(name)
    data.from_pydata(positions, [], triangles)
    data.update()
    if data.validate(verbose=True):
        raise ValueError(f"{name} mesh required repairs")
    color = data.color_attributes.new(name="PartColor", type="FLOAT_COLOR", domain="POINT")
    for vertex, rgba in zip(color.data, colors):
        vertex.color = rgba
    # A nondegenerate UV map lets UE compute tangents while retaining the facet normals.
    uv = data.uv_layers.new(name="UVMap")
    for face in data.polygons:
        normal_axis = max(range(3), key=lambda axis: abs(face.normal[axis]))
        u_axis, v_axis = (axis for axis in range(3) if axis != normal_axis)
        face.use_smooth = False
        for corner in face.loop_indices:
            point = data.vertices[data.loops[corner].vertex_index].co
            uv.data[corner].uv = (point[u_axis] / 100, point[v_axis] / 100)
    data.materials.append(bpy.data.materials.new("M_SkeletalFaceted"))

    mesh = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(mesh)
    for bone, indices in bone_vertices.items():
        mesh.vertex_groups.new(name=bone).add(indices, 1.0, "REPLACE")
    modifier = mesh.modifiers.new("RTSRig", "ARMATURE")
    modifier.object = armature
    mesh.parent = armature
    mesh.select_set(True)
    return len(positions), len(triangles)


def export_model(source, output):
    model = read_model(source)
    asset = "SK_" + Path(source).stem.replace("@", "__")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene_units = bpy.context.scene.unit_settings
    scene_units.system = "METRIC"
    scene_units.scale_length = 0.01
    rig = get_rig(model)
    armature = make_armature(rig)
    vertex_count, triangle_count = make_mesh(asset, model, rig, armature)
    fbx_path = output / f"{asset}.fbx"
    result = bpy.ops.export_scene.fbx(
        filepath=str(fbx_path),
        use_selection=True,
        object_types={"ARMATURE", "MESH"},
        axis_forward="Y", axis_up="Z",
        apply_scale_options="FBX_SCALE_UNITS",
        apply_unit_scale=True,
        primary_bone_axis="Y", secondary_bone_axis="X",
        armature_nodetype="NULL", add_leaf_bones=False,
        bake_anim=False, mesh_smooth_type="FACE", colors_type="LINEAR",
    )
    if "FINISHED" not in result or not fbx_path.is_file():
        raise RuntimeError(f"Blender failed to export {fbx_path}")
    return {
        "asset": asset,
        "source": source,
        "fbx": fbx_path.name,
        "bounds_m": model["bounds"],
        "rig": rig,
        "animation": model.get("animation", {}),
        "vertices": vertex_count,
        "triangles": triangle_count,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--key", action="append", help="Animated key in models.json; repeat for multiple models")
    selection.add_argument("--all-units", action="store_true", help="Export all unique unit models and their variants")
    selection.add_argument("--all-animated", action="store_true", help="Export all unique unit and animated resource models")
    parser.add_argument("--output", type=Path, default=BAKE_ROOT, help="Directory for FBX files and skeletal-models.json")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    entries = [export_model(source, output) for source in model_paths(args.key, args.all_units, args.all_animated)]
    manifest = output / "skeletal-models.json"
    pending = manifest.with_suffix(".tmp")
    pending.write_text(json.dumps({"version": 1, "models": entries}, indent=2) + "\n", encoding="utf-8")
    pending.replace(manifest)
    print(f"RTS_SKELETAL_EXPORT {len(entries)} meshes in {manifest}", flush=True)


if __name__ == "__main__":
    main()
