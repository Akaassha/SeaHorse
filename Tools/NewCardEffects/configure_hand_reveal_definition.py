"""Create Bodgy's private hand-reveal definition; do not change any deck."""
import unreal

root = '/Game/SeaHorse/Cards/Definitions'
name = 'Card_BodgyShaoLiMaster'
path = root + '/' + name
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError('Definition already exists; preserve designer changes: ' + path)

factory = unreal.BlueprintFactory()
factory.set_editor_property('parent_class', unreal.CardDefinition)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, root, unreal.Blueprint, factory)
assert asset
cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
cdo.set_editor_property('card_name', 'Bodgy – Mistrz Shao Li')
cdo.set_editor_property('skill_desc', 'Wybrany gracz musi pokazać ci swoją talię.')
cdo.set_editor_property('card_textrue', unreal.load_asset('/Game/SeaHorse/Cards/Textrues/T_Bodgy_Shaoli_Master'))
effect = unreal.new_object(unreal.RevealHandEffectFragment, outer=cdo)
effect.set_editor_property('effect_task_class', unreal.RevealHandEffectTask)
effect.set_editor_property('effect_presentation_id', 'Default')
effect.set_editor_property('activation_vfx', unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle'))
effect.set_editor_property('reveal_pawn_class', unreal.SHHandRevealPawn)
effect.set_editor_property('reveal_widget_class', unreal.HandRevealWidget)
cdo.set_editor_property('card_fragments', [effect])
assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
unreal.log('HAND_REVEAL_DEFINITION_CREATED ' + path)
