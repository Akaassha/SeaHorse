import unreal
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level.load_level('/Game/SeaHorse/Maps/L_Game_SixPlayers')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
order = ['BP_Hand_C_1', 'BP_Hand_C_6', 'BP_Hand_C_7', 'BP_Hand_C_8', 'BP_Hand_C_9', 'BP_Hand_C_10']
hands = {a.get_name(): a for a in actors if isinstance(a, unreal.SHHand)}
assert set(hands) == set(order), 'Unexpected map hands; inspect before migrating'
for seat, name in enumerate(order):
    hands[name].set_editor_property('layout_seat_index', seat)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
settings = world.get_world_settings()
gm = settings.get_editor_property('default_game_mode')
unreal.log_warning('[SIX] GameMode=' + str(gm))
if not gm:
    bp = unreal.load_asset('/Game/SeaHorse/Core/BP_SHGameMode')
    assert bp, 'Missing gameplay GameMode'
    settings.set_editor_property('default_game_mode', bp.generated_class())
assert level.save_current_level()
for seat, name in enumerate(order):
    unreal.log_warning('[SIX] Saved '+name+' seat='+str(hands[name].get_editor_property('layout_seat_index')))
