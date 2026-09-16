import unreal
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mat=unreal.load_asset('/Game/SeaHorse/VFX/MagicCircle/MI_MagicCircle_Preview')
rt=unreal.RenderingLibrary.create_render_target2d(world,1024,1024,unreal.TextureRenderTargetFormat.RTF_RGBA8)
unreal.RenderingLibrary.clear_render_target2d(world,rt,unreal.LinearColor(0.006,0.009,0.018,1))
unreal.RenderingLibrary.draw_material_to_render_target(world,rt,mat)
folder=str(Path(__file__).parent.resolve())
unreal.RenderingLibrary.export_render_target(world,rt,folder,'MagicCircle_Preview.png')
unreal.log('MAGIC_CIRCLE_PREVIEW_SUCCESS')

