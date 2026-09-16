import unreal

ROOT = '/Game/SeaHorse/Cards/Definitions'
UI_ROOT = '/Game/SeaHorse/Widgets'
rows = [
    ('Card_GieselbrechtApologist', 'Gieselbrecht – Apologeta',
     'Po aktywacji wybranej pary, zanim trafi na Stos Zwycięstwa, dodajesz ją do własnej Strefy. Tej zdolności można użyć tylko poza swoją kolejką.',
     unreal.CardReactionKind.CAPTURE_AFTER_ACTIVATION, 'WBP_GieselbrechtApologistReaction'),
    ('Card_GieselbrechtWizardApprentice', 'Gieselbrecht – Uczeń czarodzieja',
     'Anulujesz działanie jednej aktywacji. Tej zdolności można użyć tylko poza swoją kolejką.',
     unreal.CardReactionKind.CANCEL_ACTIVATION, 'WBP_GieselbrechtWizardApprenticeReaction')
]

for name, title, desc, kind, widget_name in rows:
    widget_path = UI_ROOT + '/' + widget_name
    if not unreal.EditorAssetLibrary.does_asset_exist(widget_path):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property('parent_class', unreal.CardReactionPrompt)
        assert unreal.AssetToolsHelpers.get_asset_tools().create_asset(widget_name, UI_ROOT, unreal.WidgetBlueprint, factory)
    unreal.EditorAssetLibrary.save_asset(widget_path, only_if_is_dirty=False)
    assert unreal.CardEffectsEditorLibrary.upgrade_reaction_prompt_blueprint(widget_path)
    widget_class = unreal.EditorAssetLibrary.load_blueprint_class(widget_path)
    path = ROOT + '/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.CardDefinition)
        assert unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.Blueprint, factory)
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
    cdo.set_editor_property('card_name', title)
    cdo.set_editor_property('skill_desc', desc)
    rules = unreal.new_object(unreal.CardActivationRulesFragment, outer=cdo)
    rules.set_editor_property('turn_restriction', unreal.CardActivationRules.OUTSIDE_OWN_TURN)
    reaction = unreal.new_object(unreal.CardReactionFragment, outer=cdo)
    reaction.set_editor_property('reaction_kind', kind)
    reaction.set_editor_property('prompt_widget_class', widget_class)
    reaction.set_editor_property('effect_presentation_id', 'Default')
    reaction.set_editor_property('activation_vfx', unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle'))
    cdo.set_editor_property('card_fragments', [rules, reaction])
    assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log_warning('[REACTIONS] Configured ' + name + ' with ' + widget_name)
