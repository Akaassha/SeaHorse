"""Read-only inventory of saved card definitions and their native/BP task classes."""
import json
from pathlib import Path
import unreal

rows = []
for path in unreal.EditorAssetLibrary.list_assets('/Game/SeaHorse/Cards/Definitions', recursive=False):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not cls:
        continue
    cdo = unreal.get_default_object(cls)
    row = {'asset': path, 'name': str(cdo.get_editor_property('card_name')),
           'description': str(cdo.get_editor_property('skill_desc')), 'fragments': []}
    for fragment in cdo.get_editor_property('card_fragments'):
        info = {'class': fragment.get_class().get_name()}
        if isinstance(fragment, unreal.CardEffectFragment):
            task = fragment.get_editor_property('effect_task_class')
            info['task'] = task.get_path_name() if task else None
        if isinstance(fragment, unreal.CardActivationRulesFragment):
            info['can_activate'] = fragment.get_editor_property('can_be_activated')
            info['turn'] = str(fragment.get_editor_property('turn_restriction'))
            info['phases'] = [str(x) for x in fragment.get_editor_property('allowed_phases')]
        row['fragments'].append(info)
    rows.append(row)
output = Path(unreal.Paths.project_saved_dir()) / 'ContextReview' / 'PanchoDefinitions.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log_warning('[PANCHO AUDIT] Read %d definitions; no assets changed' % len(rows))
