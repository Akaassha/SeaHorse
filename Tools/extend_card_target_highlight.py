"""Reuse M_Card's existing moving edge reflections for effect targets."""
import unreal

lib = unreal.MaterialEditingLibrary
material = unreal.load_asset('/Game/SeaHorse/Cards/Materials/M_Card')

def expression(name, cls, x, y):
    obj = unreal.find_object(material, name)
    if not obj:
        obj = lib.create_material_expression(material, cls, x, y)
        obj.rename(name)
    return obj

state = expression('SH_TargetHighlightState', unreal.MaterialExpressionScalarParameter, -700, -1100)
state.set_editor_property('parameter_name', 'SH_TargetHighlightState')
state.set_editor_property('default_value', 0.0)
state.set_editor_property('use_custom_primitive_data', True)
state.set_editor_property('primitive_data_index', 20)

def override(name, code, original_name, destination_name, destination_pin):
    node = expression(name, unreal.MaterialExpressionCustom, -400, -1000)
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3 if name.endswith('Color') else unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = []
    for input_name in ['Original', 'State']:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', input_name)
        inputs.append(pin)
    if not node.get_editor_property('inputs') or str(node.get_editor_property('inputs')[0].get_editor_property('input_name')) != 'Original':
        node.set_editor_property('inputs', inputs)
    node.set_editor_property('code', code)
    original = unreal.find_object(material, original_name)
    destination = unreal.find_object(material, destination_name)
    assert original and destination
    assert lib.connect_material_expressions(original, '', node, 'Original')
    assert lib.connect_material_expressions(state, '', node, 'State')
    assert lib.connect_material_expressions(node, '', destination, destination_pin)

# State 0 preserves all Blueprint-driven behavior. State 1 suppresses ordinary
# highlights during selection; 2/3/4 are white/green/red target highlights.
override('SH_TargetHighlightColor',
         'return State < 1.5 ? Original.rgb : (State < 2.5 ? float3(1,1,1) : (State < 3.5 ? float3(0,1,0) : float3(1,0,0)));',
         'MaterialExpressionCollectionParameter_0', 'MaterialExpressionMultiply_4', 'B')
override('SH_TargetHighlightEnabled',
         'return State < 0.5 ? Original : (State > 1.5 ? 1.0 : 0.0);',
         'MaterialExpressionScalarParameter_4', 'MaterialExpressionMultiply_6', 'B')
override('SH_TargetHighlightHover',
         'return State < 0.5 ? Original : (State > 2.5 ? 10000000000.0 : 1.0);',
         'MaterialExpressionScalarParameter_5', 'MaterialExpressionMultiply_8', 'B')
lib.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log_warning('SH_CARD_TARGET_HIGHLIGHT_SAVED')
