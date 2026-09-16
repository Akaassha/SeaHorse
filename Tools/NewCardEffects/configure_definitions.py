import unreal
ROOT='/Game/SeaHorse/Cards/Definitions'
rows=[
 ('Card_BodgyVampireHunter','Bodgy – Łowca wampirów','Aktywujesz tylko w pierwszym etapie swojej kolejki. W drugim dobierasz dwie karty od tego samego gracza, lecz jedną z dobranych zwracasz.','DrawTwoReturnOneEffectTask'),
 ('Card_GniewDeadHerald','Gniew – Martwy herold','Wybrany gracz oddaje ci kartę Bodgiego - Łowcy wampirów, jeśli posiada ją w swojej talii.','TakeSpecifiedCardEffectTask'),
 ('Card_GniewLivingHerald','Gniew – Żywy herold','Wybierasz Strefę innego gracza. Zabierasz z niej Pancho, Glorię, Diego albo Otfrieda i dołączasz do własnej.','StealSelectedPairEffectTask'),
 ('Card_HansCaptain','Hans – Kapitan','Każdy gracz przekazuje pary ze swojej Strefy graczowi po prawej stronie.','RotateActivationZonesRightEffectTask'),
 ('Card_ThronriTrollSlayer','Thronri – Zabójca trolli','Usuwasz tę parę z gry wraz z inną wybraną parą.','RemoveSelectedPairEffectTask'),
 ('Card_YeHeshaNightMonk','Ye-Hesha – Nocny mnich','Zbierz talie od graczy, przetasuj, a następnie rozdaj między wszystkich uczestników.','ShuffleAllHandsEffectTask')]
classes={}
for name,title,desc,task in rows:
    path=ROOT+'/'+name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        factory=unreal.BlueprintFactory()
        factory.set_editor_property('parent_class',unreal.load_class(None,'/Script/SeaHorse.CardDefinition'))
        bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT,unreal.Blueprint,factory)
        assert bp, path
    classes[name]=unreal.EditorAssetLibrary.load_blueprint_class(path)
for name,title,desc,task in rows:
    cdo=unreal.get_default_object(classes[name])
    cdo.set_editor_property('card_name',title)
    cdo.set_editor_property('skill_desc',desc)
    fragments=[]
    if name=='Card_BodgyVampireHunter':
        rules=unreal.new_object(unreal.load_class(None,'/Script/SeaHorse.CardActivationRulesFragment'),outer=cdo)
        rules.set_editor_property('allowed_phases',[unreal.TurnPhase.FIRST_PAIRING])
        fragments.append(rules)
    fragment_type='TransferCardEffectFragment' if name=='Card_GniewDeadHerald' else ('StoredPairFilterEffectFragment' if name=='Card_GniewLivingHerald' else 'CardEffectFragment')
    effect=unreal.new_object(unreal.load_class(None,'/Script/SeaHorse.'+fragment_type),outer=cdo)
    effect.set_editor_property('effect_task_class',unreal.load_class(None,'/Script/SeaHorse.'+task))
    effect.set_editor_property('effect_presentation_id','Default')
    effect.set_editor_property('activation_vfx',unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle'))
    if name=='Card_GniewDeadHerald': effect.set_editor_property('card_definition_to_transfer',classes['Card_BodgyVampireHunter'])
    if name=='Card_GniewLivingHerald':
        paths=[ROOT+'/'+n+'.'+n+'_C' for n in ['Card_Pancho','Card_Gloria','Card_Diego','Card_Otfried']]
        effect.set_editor_property('allowed_card_definitions',[unreal.SoftClassPath(p) for p in paths])
    fragments.append(effect)
    cdo.set_editor_property('card_fragments',fragments)
    unreal.EditorAssetLibrary.save_asset(ROOT+'/'+name,only_if_is_dirty=False)
    unreal.log_warning('[NEW_EFFECTS] Configured '+name)

