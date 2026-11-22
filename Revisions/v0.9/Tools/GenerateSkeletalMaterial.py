"""Create the skinned vertex-color material in the v0.9 content-only cook project."""

from pathlib import Path

import unreal


DIRECTORY = "/Game/Art/Materials"
NAME = "M_SkinnedFaceted"


def generate():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    if project != (Path(__file__).resolve().parent / "CookProject").resolve():
        raise RuntimeError("Use Tools/CookProject/Chronicles.uproject")

    editing = unreal.MaterialEditingLibrary
    assets = unreal.EditorAssetLibrary
    path = f"{DIRECTORY}/{NAME}"
    if assets.does_asset_exist(path):
        material = assets.load_asset(path)
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        material = tools.create_asset(NAME, DIRECTORY, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Expected a material at {path}")

    editing.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_skeletal_mesh", True)

    def expression(kind, x, y, **properties):
        node = editing.create_material_expression(material, kind, x, y)
        for key, value in properties.items():
            node.set_editor_property(key, value)
        return node

    def connect(source, target, input_name, output_name=""):
        if not editing.connect_material_expressions(source, output_name, target, input_name):
            raise RuntimeError(f"Cannot connect {source} to {target}.{input_name}")

    def connect_property(source, material_property):
        if not editing.connect_material_property(source, "", material_property):
            raise RuntimeError(f"Cannot connect {source} to {material_property}")

    color = expression(unreal.MaterialExpressionVertexColor, -1200, -100)
    team_color = expression(unreal.MaterialExpressionVectorParameter, -1200, 90,
                            parameter_name="TeamColor", default_value=unreal.LinearColor(1, 1, 1, 1))
    team_weight = expression(unreal.MaterialExpressionScalarParameter, -1200, 330,
                             parameter_name="TeamWeight", default_value=0.0)
    team_mask = expression(unreal.MaterialExpressionMultiply, -950, 300)
    connect(color, team_mask, "A", "A")
    connect(team_weight, team_mask, "B")

    team = expression(unreal.MaterialExpressionLinearInterpolate, -720, -100)
    connect(color, team, "A")
    connect(team_color, team, "B")
    connect(team_mask, team, "Alpha")

    stealth_color = expression(unreal.MaterialExpressionConstant3Vector, -720, 90,
                               constant=unreal.LinearColor(0.16, 0.27, 0.29, 1))
    stealth_amount = expression(unreal.MaterialExpressionScalarParameter, -720, 280,
                                parameter_name="StealthAmount", default_value=0.0)
    stealth = expression(unreal.MaterialExpressionLinearInterpolate, -480, -100)
    connect(team, stealth, "A")
    connect(stealth_color, stealth, "B")
    connect(stealth_amount, stealth, "Alpha")

    visibility_tint = expression(unreal.MaterialExpressionScalarParameter, -480, 110,
                                 parameter_name="VisibilityTint", default_value=1.0)
    base = expression(unreal.MaterialExpressionMultiply, -255, -100)
    connect(stealth, base, "A")
    connect(visibility_tint, base, "B")
    connect_property(base, unreal.MaterialProperty.MP_BASE_COLOR)

    emissive = expression(unreal.MaterialExpressionMultiply, -80, 80, const_b=0.12)
    connect(base, emissive, "A")
    connect_property(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    roughness = expression(unreal.MaterialExpressionConstant, -80, 250, r=0.9)
    connect_property(roughness, unreal.MaterialProperty.MP_ROUGHNESS)

    editing.recompile_material(material)
    if not assets.save_loaded_asset(material, False):
        raise RuntimeError(f"Cannot save {path}")
    unreal.log(f"RTS_SKELETAL_MATERIAL {path}")


if __name__ == "__main__":
    generate()
