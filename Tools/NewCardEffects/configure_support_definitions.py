import unreal

ROOT = '/Game/SeaHorse/Cards/Definitions'
ROWS = [
    ('Card_DachshundsSpectralHounds', 'Jamniki – Upiorne ogary szorstkowłose',
     'Znajdując się z Gieselbrechtem w tej samej Strefie, po jego aktywacji wędrują na Stos Zwycięstwa zamiast niego.'),
    # This path is already listed in the Living Herald's allowed definitions.
    ('Card_Pancho', 'Pancho – Arcykapłan',
     'Efekt aktywacji wybranej pary w twojej Strefie zostaje przeprowadzony podwójnie w ramach tej samej kolejki.'),
]

for name, title, description in ROWS:
    path = ROOT + '/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.CardDefinition)
        assert unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.Blueprint, factory)
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
    cdo.set_editor_property('card_name', title)
    cdo.set_editor_property('skill_desc', description)
    # Preserve unrelated fragments and presentation customizations on a rerun.
    fragments = [f for f in cdo.get_editor_property('card_fragments') if not isinstance(
        f, (unreal.CardActivationRulesFragment, unreal.VictorySubstituteFragment, unreal.CardEffectFragment))]
    rules = unreal.new_object(unreal.CardActivationRulesFragment, outer=cdo)
    if name == 'Card_Pancho':
        rules.set_editor_property('turn_restriction', unreal.CardActivationRules.OWN_TURN)
        rules.set_editor_property('allowed_phases', [unreal.TurnPhase.FIRST_PAIRING, unreal.TurnPhase.SECOND_PAIRING])
        effect = next((f for f in cdo.get_editor_property('card_fragments') if isinstance(f, unreal.CardEffectFragment)), None)
        if not effect:
            effect = unreal.new_object(unreal.CardEffectFragment, outer=cdo)
            effect.set_editor_property('effect_presentation_id', 'Default')
            effect.set_editor_property('activation_vfx', unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle'))
        effect.set_editor_property('effect_task_class', unreal.DoubleStoredPairEffectTask)
        fragments.append(effect)
    else:
        rules.set_editor_property('can_be_activated', False)
        passive = unreal.new_object(unreal.VictorySubstituteFragment, outer=cdo)
        definitions = ['Card_GieselbrechtApologist', 'Card_GieselbrechtWizardApprentice']
        passive.set_editor_property('allowed_card_definitions', [unreal.SoftClassPath(ROOT+'/'+d+'.'+d+'_C') for d in definitions])
        fragments.append(passive)
    fragments.append(rules)
    cdo.set_editor_property('card_fragments', fragments)
    assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log_warning('[SUPPORT] Configured ' + name)

unreal.log_warning('[SUPPORT] Card definitions saved; deck unchanged')
