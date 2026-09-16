# New card definitions

The six Blueprint definitions are saved under `/Game/SeaHorse/Cards/Definitions`.
They are not added to any deck automatically.

| Blueprint | Effect task |
| --- | --- |
| Card_BodgyVampireHunter | DrawTwoReturnOneEffectTask |
| Card_GniewDeadHerald | TakeSpecifiedCardEffectTask |
| Card_GniewLivingHerald | StealSelectedPairEffectTask |
| Card_HansCaptain | RotateActivationZonesRightEffectTask |
| Card_ThronriTrollSlayer | RemoveSelectedPairEffectTask |
| Card_YeHeshaNightMonk | ShuffleAllHandsEffectTask |

Bodgy is restricted to FirstPairing. The return selection offers only cards drawn
by the effect; a source containing one card receives that card back. Return is
mandatory and cannot be cancelled after drawing.

Dead Herald targets another human or a BN stack without revealing whether they
hold Bodgy. After resolving, a targeted BN stack is shuffled even if Bodgy was
absent. Living Herald directly selects a card in any opponent's eligible stored
pair, without first selecting the player. Its filter references Card_Gloria,
Card_Otfried, and the future Card_Pancho and Card_Diego definitions in the same
directory. If those future assets use different names, update the filter paths.
Living Herald can activate only while an opponent has an eligible ready pair.
If the target disappears before resolution, Herald remains in its activation zone.

Hans moves ready stored pairs to the previous human seat, skipping BN. His own
consumed pair goes to his activator's victory stack. Thronri removes both pairs
without awarding victory points; without another eligible pair he remains ready.
Ye-Hesha redistributes all hand cards, including BN, as evenly as possible;
activation and victory zones are preserved. Hans and Ye-Hesha wait for pending
additional draws before resolving.

The Blueprints use the existing Default/MagicCircle presentation. Card artwork
is left unset. `configure_definitions.py` recreates the initial definitions;
rerunning it replaces their names, descriptions and fragment configuration.

Automation coverage: `SeaHorse.Gameplay.Effects.SixNewAbilities`.

## Kurt, Paulus Wu and Ratfolk

`configure_expansion_definitions.py` creates Card_KurtPriest,
Card_PaulusWitchHunterWu and Card_RatfolkUnderground, without changing the deck.
It also migrates BP_Hand.CheckPairCompatibility to the shared native pairing rule.

Kurt selects 1–3 hand cards by clicking; another click deselects. Selecting the
third card confirms automatically; Enter confirms a smaller offer. Select a human or BN recipient, then click to draw back the
same number after the recipient's hand is shuffled. BN draws always take the
stack top. The ordinary turn draw is unchanged. Cancellation is allowed before
the transfer is committed; empty hands do not consume Kurt's pair.

Paulus Wu grants replicated protection until the start of the owner's next
actual turn. Protected players cannot be drawn from or targeted by activations;
bulk hand/zone rotations, redeals and collection effects skip them.

Ratfolk pair only with a definition in AllowedPartners (currently Paulus Silent
and Paulus Witch Hunter Wu). Pairing works in either order and awards a victory
pair immediately without starting either Paulus effect. The other copy of that
same Paulus variant is removed wherever it is held. If it was in an activation
pair, its surviving partner returns to that hand. Add future Paulus definitions
to AllowedPartners when authoring them. New card artwork remains unset.

Automation coverage: `SeaHorse.Gameplay.Effects.ExchangeProtectionAndPaulusPairing`.
