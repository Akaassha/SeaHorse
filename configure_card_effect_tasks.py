import unreal


def load_native_class(path):
    cls = unreal.load_class(None, path)
    if not cls:
        raise RuntimeError('Missing native class: ' + path)
    return cls


def card_cdo(asset_path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
    if not cls:
        raise RuntimeError('Missing CardDefinition blueprint: ' + asset_path)
    return unreal.get_default_object(cls)


def effect_fragment(cdo, task_path):
    fragment_class = load_native_class('/Script/SeaHorse.CardEffectFragment')
    fragment = unreal.new_object(fragment_class, outer=cdo)
    fragment.set_editor_property('effect_task_class', load_native_class(task_path))
    return fragment


definitions = '/Game/SeaHorse/Cards/Definitions/'

# Aramdila: every hand is passed to the player at the next seat (left).
aramdila_path = definitions + 'Card_Aramdila'
aramdila = card_cdo(aramdila_path)
aramdila.set_editor_property('card_fragments', [
    effect_fragment(aramdila, '/Script/SeaHorse.RotateHandsLeftEffectTask')
])
unreal.EditorAssetLibrary.save_asset(aramdila_path, only_if_is_dirty=False)

# Gloria: activating player chooses who loses their next turn.
gloria_path = definitions + 'Card_Gloria'
gloria = card_cdo(gloria_path)
gloria.set_editor_property('card_fragments', [
    effect_fragment(gloria, '/Script/SeaHorse.SkipSelectedPlayerTurnEffectTask')
])
unreal.EditorAssetLibrary.save_asset(gloria_path, only_if_is_dirty=False)

# Gnushor: collect every pair currently waiting in all activation zones.
gnushor_path = definitions + 'Card_Gnushor'
gnushor = card_cdo(gnushor_path)
gnushor.set_editor_property('card_fragments', [
    effect_fragment(gnushor, '/Script/SeaHorse.CollectAllActivationPairsEffectTask')
])
unreal.EditorAssetLibrary.save_asset(gnushor_path, only_if_is_dirty=False)

# Thronri: cannot activate; its pair is worth two points while left in the zone.
thronri_path = definitions + 'Card_ThronriWerewolf'
thronri = card_cdo(thronri_path)
activation_class = load_native_class('/Script/SeaHorse.CardActivationRulesFragment')
activation = unreal.new_object(activation_class, outer=thronri)
activation.set_editor_property('can_be_activated', False)
end_rules_class = load_native_class('/Script/SeaHorse.CardEndGameRulesFragment')
end_rules = unreal.new_object(end_rules_class, outer=thronri)
end_rules.set_editor_property('bonus_victory_points_per_pair_in_activation_zone', 2)
thronri.set_editor_property('card_fragments', [activation, end_rules])
unreal.EditorAssetLibrary.save_asset(thronri_path, only_if_is_dirty=False)

# Wilhelm: give the Sea Horse card from the activator's hand to a chosen player.
wilhelm_path = definitions + 'Card_Wilhelm'
wilhelm = card_cdo(wilhelm_path)
transfer_class = load_native_class('/Script/SeaHorse.TransferCardEffectFragment')
transfer = unreal.new_object(transfer_class, outer=wilhelm)
transfer.set_editor_property(
    'effect_task_class',
    load_native_class('/Script/SeaHorse.TransferSpecifiedCardEffectTask'))
transfer.set_editor_property(
    'card_definition_to_transfer',
    unreal.EditorAssetLibrary.load_blueprint_class(definitions + 'Card_SeaHorse'))
wilhelm.set_editor_property('card_fragments', [transfer])
unreal.EditorAssetLibrary.save_asset(wilhelm_path, only_if_is_dirty=False)

# Kurt: cannot activate; grants one point and wins a highest-score tie while in the zone.
kurt_path = definitions + 'Card_KurtSwordMaster'
kurt = card_cdo(kurt_path)
kurt_activation = unreal.new_object(activation_class, outer=kurt)
kurt_activation.set_editor_property('can_be_activated', False)
kurt_end_rules = unreal.new_object(end_rules_class, outer=kurt)
kurt_end_rules.set_editor_property('bonus_victory_points_per_pair_in_activation_zone', 1)
kurt_end_rules.set_editor_property('wins_score_ties', True)
kurt.set_editor_property('card_fragments', [kurt_activation, kurt_end_rules])
unreal.EditorAssetLibrary.save_asset(kurt_path, only_if_is_dirty=False)

# Olga: choose another stored pair and collect it without activating its effect.
olga_path = definitions + 'Card_OlgaPriest'
olga = card_cdo(olga_path)
olga.set_editor_property('card_fragments', [
    effect_fragment(olga, '/Script/SeaHorse.CollectSelectedActivationPairEffectTask')
])
unreal.EditorAssetLibrary.save_asset(olga_path, only_if_is_dirty=False)

unreal.log_warning(
    'Configured Aramdila, Gloria, Gnushor, Thronri, Wilhelm, Kurt and Olga fragments.')
