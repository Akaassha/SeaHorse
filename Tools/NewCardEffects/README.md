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

The old native Slate selection overlay is removed. An effect can optionally set
`CardEffectFragment.SelectionWidgetClass` to a Widget Blueprint derived from
`CardSelectionPrompt`; leaving it empty shows no selection panel. The native base
is abstract and has no fixed layout. Use `OnSelectionChanged` to refresh the UI,
`GetSelectedCardCount`, `GetMinimumCards`, `GetMaximumCards`, `GetSelectedCards`
and `GetCandidateCards` for presentation, and `CanConfirmSelection` to enable a
button calling `ConfirmSelection`. Optional custom card controls can call
`ToggleCardSelection` and `ClearSelection`. The widget closes between selection
steps and on cancellation; old widget references cannot confirm a newer request.
Selection state stays in the controller and the server validates every submission.
Table clicks, automatic confirmation of three cards, and Enter for fewer cards
continue to work with or without the widget. Use a root with visibility
`Not Hit-Testable (Self Only)` if the panel should let clicks through to the table.

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
bulk hand rotations, redeals and collection effects skip them. Hans is an
exception: his zone rotation includes protected human players as well.

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
The Apprentice is offered before an effect executes; the Apologist is offered
only after the effect and its presentation finish, immediately before the pair's
victory move. An accepted reaction opens a fresh counter-only response window
targeting that reaction pair. A successfully completed reaction can subsequently
receive its own capture window. Already accepted pairs are reserved in their zones
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
executing its effect. The Apologist's decision waits for the entire original effect,
including mandatory selections, returns, additional draws and both executions
from Pancho. If accepted, the player receives the pair ready in their
own activation zone instead of its normal victory destination. The Apologist can
also capture a reaction pair: that reaction resolves before its pair transfers.
Countering the Apologist prevents the capture without undoing the original effect.
Cancelled activations and pairs removed from the game or kept on the table by
their own effects receive no capture window. Eligible Apologists are determined
from the table after the effect (including Hans's zone rotation). Uncaptured
reaction pairs are consumed once; a successful Gieselbrecht can use Jamniki as
its victory substitute after the capture decision.

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

## Jamniki and Pancho

`configure_support_definitions.py` creates these definitions without modifying the deck:

- `Card_DachshundsSpectralHounds`: Jamniki – Upiorne ogary szorstkowłose.
- `Card_Pancho`: Pancho – Arcykapłan. This path already belongs to the Living
  Herald's allowed pair definitions.

Jamniki use `VictorySubstituteFragment.AllowedCardDefinitions`, configured for
both Gieselbrecht variants. They are passive, not manually activatable. After a
successful Gieselbrecht activation, the oldest ready Jamniki pair in the same
zone goes to victory and Gieselbrecht becomes ready again after presentation.
A cancelled Gieselbrecht does not consume Jamniki. Capturing Gieselbrecht takes
precedence: he changes zones instead of paying a victory cost, so Jamniki stay.
Add future Gieselbrecht definitions to the fragment's allowed list.

Pancho uses `DoubleStoredPairEffectTask`. Select a pair directly in your own
activation zone. Pancho goes to victory; the selected pair remains ready and
is not automatically activated. Candidates must be currently legal to activate,
have a task-based effect, and not already have the bonus. Thus passive and
outside-turn reaction pairs cannot be selected. The replicated
`FActivatedPair.bDoubleEffectThisTurn` is Blueprint-readable and expires at turn
end, including on pairs transferred to another zone.

Activating the selected pair opens one normal reaction window. If it is not
cancelled, its effect runs twice in sequence, with fresh target selection each
time. The pair is consumed once after both executions. Capture waits for both
executions; a counter cancels the entire activation. Cancelling initial target
selection with ESC preserves the bonus; the second execution cannot roll back
the first. If the second execution has no target, a successful first execution
still consumes the pair. Self-removing effects retain their final removal rule.
Bodgy performs two draw-and-return sequences (four draws and two returns when
enough cards are available), with separate return candidates for each sequence.

Automation coverage: `SeaHorse.Gameplay.Effects.SupportPairs`, including
reaction chains, capture, presentation locks, repeated targeting, deferred
draws, bulk collection, removal, and expiry.

Each effect execution resets only its VFX notification cache via
`MulticastBeginPairEffectExecution`; duplicate VFX notifications within that
execution still produce one circle. A repeated Hans additionally waits
`RotateActivationZonesRightEffectTask.TransferPresentationDuration` (1.5 seconds)
after the first transfer before starting the second circle. The activation stays
pending during this interval, while local card movement remains enabled.
`SeaHorse.Gameplay.Effects.DoubledZoneRotation` verifies each intermediate and
final destination for 2–6 human players, including BN and protected seats.

Before any repeated task starts, pending presentation locks must finish and
their victory moves must flush. In particular, Olga's second selection excludes
pairs already sent to victory. `PanchoAllDefinitions` exercises all 18 saved
activatable card definitions, including `BP_DefaultCardEffect`. See
`PanchoCompatibility.md` for the checked outcomes and limits. The read-only
`audit_pancho_definitions.py` exports the current card/task inventory to
`Saved/ContextReview/PanchoDefinitions.json` without saving assets.
