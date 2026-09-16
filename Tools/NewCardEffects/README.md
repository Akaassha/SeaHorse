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

## Gieselbrecht reactions

`configure_reaction_definitions.py` creates `Card_GieselbrechtApologist` and
`Card_GieselbrechtWizardApprentice`, without changing the deck. Their reaction
fragments use `WBP_GieselbrechtApologistReaction` and
`WBP_GieselbrechtWizardApprenticeReaction` in `/Game/SeaHorse/Widgets`.

Ready reaction pairs are offered only outside their owner's turn. Every eligible
player receives a prompt concurrently; the first valid acceptance received by the
server wins that response window and closes its prompts. Pair creation order and seat order give no
priority between players. If a player owns multiple eligible pairs, their prompt
offers their oldest pair first; declining offers their next pair while everyone
else can still accept. A declined or slower pair remains unspent.
An accepted reaction opens a fresh concurrent response window targeting that
reaction pair. Other eligible pairs can counter or capture it, and their reactions
can receive further reactions. Already accepted pairs are reserved in their zones
and cannot be reused in the same chain. The outside-own-turn restriction still
applies at every step, so the current turn's owner cannot use these reaction cards.
When everyone declines or no eligible pairs remain, the server resolves the chain
from the last accepted reaction back to the original activation. An uncancelled
counter cancels only its immediate target; a cancelled counter has no effect.
For example, two counters let the original effect execute, while three cancel it.
The server pauses gameplay throughout these decisions. Disconnecting a responder
removes their pending offers; if no responses remain, the chain can resolve.
Activations of protected players cannot be countered or
captured. Response IDs are bound to their player and offer, so delayed, duplicate
or forged replies cannot resolve another offer.

Closing a reaction prompt restores the gameplay Game and UI input mode with an
unlocked, visible cursor during dragging, so card dragging and the HUD phase
button remain available on the responding player's next turn.

The Apprentice sends the cancelled pair to its owner's victory stack without
executing its effect. The Apologist lets the original effect finish, including
mandatory selections and additional draws, then receives the pair ready in their
own activation zone instead of its normal victory destination. The Apologist can
also capture a reaction pair: that reaction resolves before its pair transfers.
Countering the Apologist prevents the capture. A pair removed from the game by
its own effect cannot be captured. Accepted reaction pairs, including cancelled
ones, are consumed once to their owners' victory stacks unless captured by a
later reaction.

To customize either prompt, assign a subclass of `UCardReactionPrompt` to
`CardReactionFragment.PromptWidgetClass` in the card definition. The native base
is abstract and has no layout; the supplied widget Blueprints contain an editable
UMG hierarchy with a Polish question and Tak/Nie buttons. Open their Designer to
change it, or create a new Widget Blueprint directly derived from
`CardReactionPrompt`. Optional buttons named `AcceptButton` and `DeclineButton`
are wired automatically; an optional text block named `QuestionText` receives
the default question. For other control names, bind OnClicked to inherited
`AcceptReaction` and `DeclineReaction`. Use `OnOfferPresented`, `ReactionCard`,
and `TargetCard` for custom content. A missing widget class declines the offer
and logs a warning. `upgrade_reaction_widgets.py` migrates only the original
empty prompts and preserves existing custom layouts.
Widgets only present the offer and submit the decision;
the server validates and resolves it. Rerunning the configuration script
replaces the initial card fragment configuration.

Automation coverage: `SeaHorse.Gameplay.Effects.OutOfTurnReactions`.
