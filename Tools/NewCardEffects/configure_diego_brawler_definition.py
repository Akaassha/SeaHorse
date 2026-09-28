"""Create Diego's hand-comparison card definition; do not change any deck."""
import unreal

root = '/Game/SeaHorse/Cards/Definitions'
name = 'Card_Diego'
path = root + '/' + name
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError('Definition already exists; preserve designer changes: ' + path)

legacy_path = root + '/Card_DiegoBrawler'
if unreal.EditorAssetLibrary.does_asset_exist(legacy_path):
    if not unreal.EditorAssetLibrary.rename_asset(legacy_path, path):
        raise RuntimeError('Could not rename the temporary Diego definition')
    asset = unreal.EditorAssetLibrary.load_asset(path)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.CardDefinition)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, root, unreal.Blueprint, factory)
    assert asset

cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
cdo.set_editor_property('card_name', 'Diego – Zwadźca')
cdo.set_editor_property(
    'skill_desc',
    'Z wybranym graczem porównujecie liczbę kart w waszych taliach. '
    'Nadmiar zostaje dobrany przez gracza z ich mniejszą ilością.')
cdo.set_editor_property('card_textrue', unreal.load_asset('/Game/SeaHorse/Cards/Textrues/T_Diego'))
effect = unreal.new_object(unreal.CompareHandsEffectFragment, outer=cdo)
effect.set_editor_property('effect_task_class', unreal.CompareHandsEffectTask)
effect.set_editor_property('effect_presentation_id', 'Default')
effect.set_editor_property('activation_vfx', unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle'))
effect.set_editor_property(
    'reveal_pawn_class',
    unreal.EditorAssetLibrary.load_blueprint_class('/Game/SeaHorse/Cards/BP_HandRevealPawn'))
effect.set_editor_property('reveal_widget_class', unreal.HandRevealWidget)
cdo.set_editor_property('card_fragments', [effect])
assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
unreal.log('DIEGO_DEFINITION_CREATED ' + path)
