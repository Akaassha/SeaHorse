import unreal
from pathlib import Path

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
system=unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle')
actor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.NiagaraActor,unreal.Vector(0,0,300))
component=actor.get_component_by_class(unreal.NiagaraComponent)
component.set_asset(system)
component.set_force_solo(True)
component.activate(True)
component.advance_simulation(45,1.0/60.0)
active_during=component.is_active()
component.advance_simulation(180,1.0/60.0)
active_after=component.is_active()
Path(__file__).with_name('niagara_validation.txt').write_text('active_at_0.75s='+str(active_during)+'\nactive_at_3.75s='+str(active_after)+'\n')
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)
if not active_during or active_after:
    raise RuntimeError('Niagara must be active during its lifetime and finish automatically afterwards')
unreal.log('MAGIC_CIRCLE_NIAGARA_LIFETIME_SUCCESS')
