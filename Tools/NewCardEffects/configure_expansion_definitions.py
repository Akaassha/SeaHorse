import unreal

ROOT = '/Game/SeaHorse/Cards/Definitions'
ROWS = [
    ('Card_KurtPriest', 'Kurt – Kapłan', 'Wybierasz od jednej do trzech kart i przekazujesz je innemu graczowi, który następnie tasuje swoją talię. Dobierasz od niego taką samą liczbę kart.', 'ExchangeHandCardsEffectTask'),
    ('Card_PaulusWitchHunterWu', 'Paulus – Łowca czarownic Wu', 'Do początku twojej następnej kolejki nie można od ciebie dobierać kart i jesteś niewrażliwy na inne aktywacje.', 'ProtectUntilNextTurnEffectTask'),
    ('Card_RatfolkUnderground', 'Szczuroludzie – Mieszkańcy podziemi', 'Tę kartę można parować tylko z Paulusem. Od razu wędrują na Stos Zwycięstwa bez aktywacji Paulusa, którego druga karta zostaje usunięta z gry.', None),
]

for name, title, description, task in ROWS:
    path = ROOT + '/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/SeaHorse.CardDefinition'))
        assert unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.Blueprint, factory)
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
    cdo.set_editor_property('card_name', title)
    cdo.set_editor_property('skill_desc', description)
    fragments = []
    if task:
        effect = unreal.new_object(unreal.CardEffectFragment, outer=cdo)
        effect.set_editor_property('effect_task_class', unreal.load_class(None, '/Script/SeaHorse.' + task))
        effect.set_editor_property('effect_presentation_id', 'Default')
        effect.set_editor_property('activation_vfx', unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle'))
        fragments.append(effect)
    else:
        pairing = unreal.new_object(unreal.ImmediateVictoryPairFragment, outer=cdo)
        partners = ['Card_PaulusSilent', 'Card_PaulusWitchHunterWu']
        pairing.set_editor_property('allowed_partners', [unreal.SoftClassPath(ROOT+'/'+p+'.'+p+'_C') for p in partners])
        fragments.append(pairing)
        rules = unreal.new_object(unreal.CardActivationRulesFragment, outer=cdo)
        rules.set_editor_property('can_be_activated', False)
        fragments.append(rules)
    cdo.set_editor_property('card_fragments', fragments)
    assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log_warning('[EXPANSION] Configured ' + name)

assert unreal.CardEffectsEditorLibrary.upgrade_hand_pairing_blueprint(), 'BP_Hand pairing migration failed'
unreal.log_warning('[EXPANSION] BP_Hand uses native pairing rules; deck unchanged')
