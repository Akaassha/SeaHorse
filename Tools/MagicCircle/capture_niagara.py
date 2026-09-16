"""Render Niagara in a temporary PIE map using -RenderOffscreen. No map is saved."""
from pathlib import Path
import unreal

output_dir = str(Path(__file__).parent.resolve())
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor = actors.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(0, 0, 1000))
component = actor.get_component_by_class(unreal.NiagaraComponent)
component.set_asset(unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle'))
component.deactivate()
control = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(24, 0, 1000))
control.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/SM_MagicCircle_Preview'))
control.set_actor_scale3d(unreal.Vector(0.1, 0.1, 0.1))
camera = actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 1100), unreal.Rotator(pitch=-90))
capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
capture.set_editor_property('fov_angle', 45.0)
capture.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture.set_editor_property('capture_every_frame', False)
capture.set_editor_property('always_persist_rendering_state', True)
capture.set_editor_property('show_flag_settings', [unreal.EngineShowFlagsSetting(show_flag_name='EyeAdaptation', enabled=False)])
rt = unreal.RenderingLibrary.create_render_target2d(world, 512, 512, unreal.TextureRenderTargetFormat.RTF_RGBA8)
capture.set_editor_property('texture_target', rt)
frames = 0
start_time = None
stage = 0
export_name = None
export_frame = 0
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def tick(delta):
    global frames, world, component, capture, start_time, stage, export_name, export_frame
    frames += 1
    try:
        if frames == 60:
            editor.editor_play_simulate()
        if frames == 180:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            assert world, 'PIE world missing'
            component = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NiagaraActor)[0].get_component_by_class(unreal.NiagaraComponent)
            capture = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)[0].get_component_by_class(unreal.SceneCaptureComponent2D)
            capture.set_editor_property('texture_target', rt)
            component.activate(True)
            start_time = unreal.GameplayStatics.get_time_seconds(world)
        if start_time is not None:
            elapsed = unreal.GameplayStatics.get_time_seconds(world) - start_time
            thresholds = [(0.3, 'Niagara_Early.png'), (0.75, 'Niagara_InScene.png'), (1.4, 'Niagara_Fading.png'), (2.0, 'Niagara_Finished.png')]
            if export_name and frames >= export_frame:
                unreal.RenderingLibrary.export_render_target(world, rt, output_dir, export_name)
                unreal.log('MAGIC_CAPTURE ' + export_name)
                export_name = None
            if stage < len(thresholds) and elapsed >= thresholds[stage][0] and not export_name:
                capture.capture_scene()
                export_name = thresholds[stage][1]
                export_frame = frames + 3
                stage += 1
            if stage == len(thresholds) and not export_name:
                assert not component.is_active(), 'Magic circle did not finish'
                unreal.log('MAGIC_CIRCLE_SCENE_CAPTURE_SUCCESS')
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        assert frames < 2400, 'Timed out waiting for PIE capture'
    except Exception:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        raise


handle = unreal.register_slate_post_tick_callback(tick)
