"""Add the two matching Diego cards to the main deck without touching test decks."""
import json
import unreal

path = '/Game/SeaHorse/Deck/DT_Deck'
table = unreal.load_asset(path)
if not table:
    raise RuntimeError('Main deck data table is missing: ' + path)

rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
if any(row.get('Name') == 'Diego' for row in rows):
    raise RuntimeError('Diego row already exists; preserve designer changes in ' + path)

rows.append({
    'Name': 'Diego',
    'CardDefinition': (
        "/Script/Engine.BlueprintGeneratedClass'"
        "/Game/SeaHorse/Cards/Definitions/Card_Diego.Card_Diego_C'"),
    'Count': 2,
})
updated = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(
    table, json.dumps(rows, ensure_ascii=False))
if not updated:
    raise RuntimeError('Could not update main deck')
if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
    raise RuntimeError('Could not save main deck: ' + path)
unreal.log('DIEGO_ADDED_TO_MAIN_DECK')
