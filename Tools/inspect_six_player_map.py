"""Read-only verification of the saved six-seat test map."""
import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/SeaHorse/Maps/L_Game_SixPlayers')
hands = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
         if isinstance(a, unreal.SHHand)]
seats = sorted(a.get_editor_property('layout_seat_index') for a in hands)
assert seats == list(range(6)), f'Invalid table seats: {seats}'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
gm = world.get_world_settings().get_editor_property('default_game_mode')
assert gm and isinstance(unreal.get_default_object(gm), unreal.SHGameMode)
unreal.log_warning(f'[SIX] VERIFIED saved seats={seats}, GameMode={gm.get_path_name()}')
