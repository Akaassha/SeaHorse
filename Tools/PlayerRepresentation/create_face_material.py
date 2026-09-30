"""Run in Unreal Editor Python. Creates the player face material; never changes Blueprints."""
import unreal

ROOT = "/Game/SeaHorse/Board/Materials"
NAME = "M_PlayerFace"
PATH = ROOT + "/" + NAME

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.log("PLAYER_FACE_MATERIAL_EXISTS: " + PATH)
else:
    unreal.EditorAssetLibrary.make_directory(ROOT)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, ROOT, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    lib = unreal.MaterialEditingLibrary

    def node(cls, x, y):
        return lib.create_material_expression(material, cls, x, y)

    face = node(unreal.MaterialExpressionTextureSampleParameter2D, -700, -100)
    face.set_editor_property("parameter_name", "PlayerFaceTexture")
    face.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    background = node(unreal.MaterialExpressionVectorParameter, -700, 250)
    background.set_editor_property("parameter_name", "BackgroundColor")
    background.set_editor_property("default_value", unreal.LinearColor(0.025, 0.014, 0.008, 1))
    inverse_alpha = node(unreal.MaterialExpressionOneMinus, -400, 120)
    assert lib.connect_material_expressions(face, "A", inverse_alpha, "")
    uncovered = node(unreal.MaterialExpressionMultiply, -180, 180)
    assert lib.connect_material_expressions(background, "", uncovered, "A")
    assert lib.connect_material_expressions(inverse_alpha, "", uncovered, "B")
    composite = node(unreal.MaterialExpressionAdd, 50, -50)
    # Slate's output is premultiplied: multiplying RGB by alpha again darkens text edges.
    assert lib.connect_material_expressions(face, "RGB", composite, "A")
    assert lib.connect_material_expressions(uncovered, "", composite, "B")
    assert lib.connect_material_property(composite, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = node(unreal.MaterialExpressionScalarParameter, 50, 280)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.85)
    assert lib.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log("PLAYER_FACE_MATERIAL_CREATED: " + PATH)
