"""Assign magic circles to targeted tasks, which start presentation after their last target."""
import unreal
import shutil
from pathlib import Path
from datetime import datetime

project=Path(unreal.Paths.project_dir()).resolve()
backup=project/'Saved/Diagnostics/MagicCircleIntegrationBackup'/datetime.now().strftime('%Y%m%d_%H%M%S')
backup.mkdir(parents=True,exist_ok=True)
expected={
    'Card_Gloria':'/Script/SeaHorse.SkipSelectedPlayerTurnEffectTask',
    'Card_OlgaPriest':'/Script/SeaHorse.CollectSelectedActivationPairEffectTask',
    'Card_PaulusSilent':'/Script/SeaHorse.ChooseDrawSourceEffectTask',
    'Card_Wilhelm':'/Script/SeaHorse.TransferSpecifiedCardEffectTask',
}
system=unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle')
assert system
pending=[]
for name,task_path in expected.items():
    path='/Game/SeaHorse/Cards/Definitions/'+name
    bp=unreal.load_asset(path)
    cdo=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
    fragments=[f for f in cdo.get_editor_property('card_fragments') if isinstance(f,unreal.CardEffectFragment)]
    assert len(fragments)==1, path
    fragment=fragments[0]
    assert fragment.get_editor_property('effect_task_class').get_path_name()==task_path, path
    pending.append((name,bp,cdo,fragment))
for name,bp,cdo,fragment in pending:
    shutil.copy2(project/'Content/SeaHorse/Cards/Definitions'/(name+'.uasset'),backup/(name+'.uasset'))
    cdo.modify()
    fragment.modify()
    fragment.set_editor_property('activation_vfx',system)
    fragment.set_editor_property('activation_vfx_duration',1.5)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False),name
    unreal.log('MAGIC_CIRCLE_CONNECTED '+name)
unreal.log('MAGIC_CIRCLE_BACKUP '+str(backup))
