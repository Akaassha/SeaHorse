"""Add card-style moving reflections to the player/NPC picker mesh overlay."""
import unreal

path = '/Game/SeaHorse/VFX/M_ParticipantTargetReflections'
lib = unreal.MaterialEditingLibrary
material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ParticipantTargetReflections', '/Game/SeaHorse/VFX', unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('two_sided', True)

def expression(name, cls, x, y):
    obj = unreal.find_object(material, name)
    if not obj:
        obj = lib.create_material_expression(material, cls, x, y)
        obj.rename(name)
    return obj

state = expression('SH_TargetHighlightState', unreal.MaterialExpressionScalarParameter, -600, 200)
state.set_editor_property('parameter_name', 'SH_TargetHighlightState')
state.set_editor_property('default_value', 0.0)
state.set_editor_property('use_custom_primitive_data', True)
state.set_editor_property('primitive_data_index', 20)
uv = expression('SH_ReflectionUV', unreal.MaterialExpressionTextureCoordinate, -600, -200)
time = expression('SH_ReflectionTime', unreal.MaterialExpressionTime, -600, 0)
node = expression('SH_ParticipantReflections', unreal.MaterialExpressionCustom, 0, 0)
node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
node.set_editor_property('description', 'M_Card moving edge reflections, target color and hover intensity')
names = ['UV', 'Time', 'State', 'Speed', 'EdgeWidth']
if [str(pin.get_editor_property('input_name')) for pin in node.get_editor_property('inputs')] != names:
    pins = []
    for name in names:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    node.set_editor_property('inputs', pins)
# Same angular cosine, power, edge falloff, speed and hover multiplier as M_Card.
# Overlay opacity is additive: State 0/1 contributes zero and leaves the base intact.
node.set_editor_property('code', '''
if (State < 1.5) return float3(0,0,0);
float2 centered = UV - 0.5;
float angle = atan2(centered.y, centered.x) + Time * Speed;
float reflection = pow(abs(cos(angle)), 10000.0);
float distance = min(min(UV.x, 1.0-UV.x), min(UV.y, 1.0-UV.y));
float edge = 1.0 - smoothstep(0.0, max(EdgeWidth, 0.0001), distance);
float3 color = State < 2.5 ? float3(1,1,1) : (State < 3.5 ? float3(0,1,0) : float3(1,0,0));
float intensity = State > 2.5 ? 10000000000.0 : 1.0;
return color * reflection * edge * 10.0 * intensity;
''')
for source, name in [(uv, 'UV'), (time, 'Time'), (state, 'State')]:
    assert lib.connect_material_expressions(source, '', node, name)
for index, (name, default) in enumerate([('Speed', 0.8), ('EdgeWidth', 0.02)]):
    param = expression('SH_' + name, unreal.MaterialExpressionScalarParameter, -600, 400 + index*160)
    param.set_editor_property('parameter_name', name)
    param.set_editor_property('default_value', default)
    assert lib.connect_material_expressions(param, '', node, name)
assert lib.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)

bp = unreal.load_asset('/Game/SeaHorse/Board/BP_PlayerRepresentation')
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
seen = set()
count = 0
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if not isinstance(obj, unreal.StaticMeshComponent) or obj.get_path_name() in seen:
        continue
    seen.add(obj.get_path_name())
    obj.set_editor_property('overlay_material', material)
    count += 1
assert count > 0, 'No participant mesh found'
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp)
unreal.log_warning('SH_NPC_REFLECTIONS_SAVED meshes=' + str(count))
