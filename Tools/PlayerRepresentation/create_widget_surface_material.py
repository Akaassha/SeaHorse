"""Create a whole-widget, unlit surface without modifying existing materials or Blueprints."""
import unreal

root = '/Game/SeaHorse/Board/Materials'
name = 'M_PlayerWidgetSurface'
path = root + '/' + name
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.log('PLAYER_WIDGET_SURFACE_EXISTS: ' + path)
else:
    unreal.EditorAssetLibrary.make_directory(root)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, root, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    lib = unreal.MaterialEditingLibrary
    texture = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    texture.set_editor_property('parameter_name', 'PlayerFaceTexture')
    texture.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
    assert lib.connect_material_property(texture, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(texture, 'A', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log('PLAYER_WIDGET_SURFACE_CREATED: ' + path)
