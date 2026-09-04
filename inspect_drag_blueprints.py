import unreal


ASSETS = [
    "/Game/SeaHorse/Cards/BP_Card",
    "/Game/SeaHorse/Cards/BP_Hand",
    "/Game/SeaHorse/Core/BP_SHPlayerController",
]

try:
    unreal.AssetToolsHelpers.get_asset_tools().export_assets(
        ASSETS, "D:/Vacui Projects/SeaHorse/SeaHorse/Saved/BlueprintExports")
    unreal.log_warning("[DRAG_INSPECT] Export requested")
except Exception as error:
    unreal.log_warning(f"[DRAG_INSPECT] Export failed: {error}")


for asset_path in ASSETS:
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not blueprint:
        unreal.log_warning(f"[DRAG_INSPECT] Missing {asset_path}")
        continue
    unreal.log_warning(f"[DRAG_INSPECT] ASSET {asset_path}")
    graphs = []
    for property_name in ("ubergraph_pages", "function_graphs", "delegate_signature_graphs"):
        try:
            graphs.extend(blueprint.get_editor_property(property_name))
        except Exception as error:
            unreal.log_warning(f"[DRAG_INSPECT] {property_name} unavailable: {error}")
    for graph in graphs:
        nodes = graph.get_editor_property("nodes")
        relevant = []
        for node in nodes:
            title = node.get_node_title(unreal.NodeTitleType.FULL_TITLE)
            text = f"{node.get_class().get_name()} {title}"
            if any(word in text.lower() for word in (
                "drag", "drop", "takecard", "dregged", "cursor", "mouse", "click"
            )):
                relevant.append(text.replace("\n", " | "))
        if relevant:
            unreal.log_warning(f"[DRAG_INSPECT] GRAPH {graph.get_name()}")
            for text in relevant:
                unreal.log_warning(f"[DRAG_INSPECT] NODE {text}")
