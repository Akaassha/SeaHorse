"""Give the two original empty prompts an editable UMG hierarchy, preserving custom layouts."""
import unreal

for name in ('WBP_GieselbrechtApologistReaction', 'WBP_GieselbrechtWizardApprenticeReaction'):
    path = '/Game/SeaHorse/Widgets/' + name
    assert unreal.CardEffectsEditorLibrary.upgrade_reaction_prompt_blueprint(path), path
    unreal.log_warning('[REACTION UMG] Ready: ' + path)
