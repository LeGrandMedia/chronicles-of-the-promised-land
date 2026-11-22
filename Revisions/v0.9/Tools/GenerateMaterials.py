"""Run with UnrealEditor-Cmd's Python commandlet in Tools/CookProject."""
import unreal

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = '/Game/Art/Materials'
name = 'M_Faceted'
material = unreal.EditorAssetLibrary.load_asset(path + '/' + name) if unreal.EditorAssetLibrary.does_asset_exist(path + '/' + name) else None
if not material:
    material = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
library.delete_all_material_expressions(material)
material.set_editor_property('two_sided', True)
color = library.create_material_expression(material, unreal.MaterialExpressionVertexColor, -300, 0)
assert library.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR), 'Vertex color base connection failed'
rough = library.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 180)
rough.set_editor_property('r', 0.9)
assert library.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS), 'Roughness connection failed'
ambient = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -100, 300)
ambient.set_editor_property('const_b', 0.12)
assert library.connect_material_expressions(color, '', ambient, 'A'), 'Vertex color ambient connection failed'
assert library.connect_material_property(ambient, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR), 'Ambient connection failed'
library.recompile_material(material)
unreal.EditorAssetLibrary.save_asset(path + '/' + name)
unreal.log('RTS_MATERIAL_GENERATED ' + path + '/' + name)
