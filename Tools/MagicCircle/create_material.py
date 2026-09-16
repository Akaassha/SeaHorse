"""Run with Unreal Editor Python. Creates only /Game/SeaHorse/VFX/MagicCircle assets."""
import unreal
from pathlib import Path

ROOT = '/Game/SeaHorse/VFX/MagicCircle'
unreal.EditorAssetLibrary.make_directory(ROOT)
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary

def asset(name, cls, factory):
    path = ROOT + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError('Refusing to overwrite existing asset: ' + path)
    return tools.create_asset(name, ROOT, cls, factory)

m = asset('M_MagicCircle', unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)
m.set_editor_property('used_with_niagara_sprites', True)

def node(cls, x, y):
    return lib.create_material_expression(m, cls, x, y)

uv = node(unreal.MaterialExpressionTextureCoordinate, -900, -200)
age = node(unreal.MaterialExpressionDynamicParameter, -900, 0)
age.set_editor_property('param_names', ['EffectAge', 'Unused1', 'Unused2', 'Unused3'])
custom = node(unreal.MaterialExpressionCustom, -350, 0)
custom.set_editor_property('description', 'Arcane rings / procedural antialiased sigils / lifetime fade')
custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
custom.set_editor_property('code', Path(__file__).with_name('magic_circle.hlsl').read_text())
inputs=[]
for name in ['UV','Age','Primary','Accent','Intensity','RotationSpeed','PreviewAge','UseParticleAge']:
    item=unreal.CustomInput()
    item.set_editor_property('input_name',name)
    inputs.append(item)
custom.set_editor_property('inputs',inputs)
lib.connect_material_expressions(uv,'',custom,'UV')
lib.connect_material_expressions(age,'EffectAge',custom,'Age')
for i,(name,value) in enumerate([('Intensity',3.0),('RotationSpeed',0.8),('PreviewAge',0.5),('UseParticleAge',1.0)]):
    p=node(unreal.MaterialExpressionScalarParameter,-900,200+i*130)
    p.set_editor_property('parameter_name',name)
    p.set_editor_property('default_value',value)
    lib.connect_material_expressions(p,'',custom,name)
for i,(name,value) in enumerate([('Primary',unreal.LinearColor(0.12,0.8,1,1)),('Accent',unreal.LinearColor(1,0.55,0.12,1))]):
    p=node(unreal.MaterialExpressionVectorParameter,-1200,200+i*170)
    p.set_editor_property('parameter_name',name)
    p.set_editor_property('default_value',value)
    lib.connect_material_expressions(p,'',custom,name)
rgb=node(unreal.MaterialExpressionComponentMask,0,-70)
for c in ['r','g','b']: rgb.set_editor_property(c,True)
rgb.set_editor_property('a',False)
alpha=node(unreal.MaterialExpressionComponentMask,0,100)
for c in ['r','g','b']: alpha.set_editor_property(c,False)
alpha.set_editor_property('a',True)
lib.connect_material_expressions(custom,'',rgb,'')
lib.connect_material_expressions(custom,'',alpha,'')
lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(m)
unreal.EditorAssetLibrary.save_loaded_asset(m)

for name,preview in [('MI_MagicCircle',False),('MI_MagicCircle_Preview',True)]:
    mi=asset(name,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi,m)
    if preview: lib.set_material_instance_scalar_parameter_value(mi,'UseParticleAge',0.0)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
mesh=tools.duplicate_asset('SM_MagicCircle_Preview',ROOT,unreal.load_asset('/Engine/BasicShapes/Plane'))
mesh.set_material(0,unreal.load_asset(ROOT+'/MI_MagicCircle_Preview'))
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
unreal.log('MAGIC_CIRCLE_MATERIAL_SUCCESS')
