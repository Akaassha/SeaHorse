"""Extend the camera's existing PP_SoftOutline; preserve its ordinary outlines."""
import unreal
from pathlib import Path

material = unreal.load_asset('/Game/SeaHorse/VFX/PP_SoftOutline')
lib = unreal.MaterialEditingLibrary
node = unreal.find_object(material, 'SH_EffectTargetOutline')
if not node:
    node = lib.create_material_expression(material, unreal.MaterialExpressionCustom, 650, -750)
    node.rename('SH_EffectTargetOutline')
node.set_editor_property('description', 'Effect targets: 250 white / 251 green / 252 red')
node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs = []
for name in ['Original', 'DetectedStencil', 'EdgeMask']:
    pin = unreal.CustomInput()
    pin.set_editor_property('input_name', name)
    inputs.append(pin)
if [str(pin.get_editor_property('input_name')) for pin in node.get_editor_property('inputs')] != ['Original', 'DetectedStencil', 'EdgeMask']:
    node.set_editor_property('inputs', inputs)
node.set_editor_property('code', Path(__file__).with_name('target_outline.hlsl').read_text())
original = unreal.find_object(material, 'MaterialExpressionAdd_3')
stencil = unreal.find_object(material, 'MaterialExpressionNamedRerouteDeclaration_2')
edge = unreal.find_object(material, 'MaterialExpressionNamedRerouteDeclaration_6')
final_scale = unreal.find_object(material, 'MaterialExpressionMultiply_0')
assert original and stencil and edge and final_scale, 'Existing outline graph has changed; inspect before migrating.'
assert lib.connect_material_expressions(original, '', node, 'Original')
assert lib.connect_material_expressions(stencil, '', node, 'DetectedStencil')
assert lib.connect_material_expressions(edge, '', node, 'EdgeMask')
assert lib.connect_material_expressions(node, '', final_scale, 'A')
assert lib.connect_material_property(final_scale, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log_warning('SH_TARGET_OUTLINE_MATERIAL_SAVED')
