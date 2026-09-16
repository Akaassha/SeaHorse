The camera continues to use `PPI_SoftOutline`, inheriting `PP_SoftOutline`.
The existing material graph supplies `DetectedStencil` and `EdgeMask` to
`SH_EffectTargetOutline`. Target colors use the active-card outline's exact edge
sampling, width, additive compositing and final intensity multiplier; there is
no separate outline kernel. The expression decodes these reserved stencil IDs:

| ID | Target presentation |
| --- | --- |
| 250 | Valid candidate, white |
| 251 | Valid candidate under cursor, green |
| 252 | Invalid card/player representation under cursor, red |

`ASHPlayerController` applies these values locally during pair effect target
selection using the server-provided candidate lists. It updates card and player
representation meshes, suppresses ordinary outlines while choosing, and restores
their custom depth settings when selection finishes or the controller ends play.
World widget quads are excluded. NPC cards are valid only when their logical NPC
hand is an eligible participant. Human participant choices use player pickers.

`extend_target_outline.py` updates the material from `target_outline.hlsl` using
Unreal Editor Python. The modified asset is already saved; running the script is
only necessary after editing the shader. IDs 250–252 must not be reused by other
custom depth effects. The original `width` parameter also controls these outlines.

ChooseDrawSource selects a human drawing player, then a participant hand as the
source (including NPC stacks). Both the NPC representation and its top card can
submit the eligible hand. The server queues that hand as the forced draw source.
Turn-based choices such as who draws or skips a turn still require a human player:
NPC stacks do not have turns. Card-transfer recipients already include NPC hands.

`M_Card` also reads reserved Custom Primitive Data slot **20**. The controller
sets 1 to suppress ordinary reflections during selection, 2 for white target
reflections, 3 for green hover and 4 for red hover. Zero preserves the existing
Blueprint-controlled material parameters. Previous per-mesh values are restored
on teardown. This does not replace the card's dynamic material or its textures.
The existing moving edge mask, Speed, EdgeWidth and base intensity remain shared;
hover uses the same HooverIntensity value (1e10) as BP_Card's existing hover.
`BP_PlayerRepresentation` uses `M_ParticipantTargetReflections` as an additive
mesh overlay, preserving the original base material. This gives NPC and human
pickers the same rotating cosine highlights, UV-edge falloff and hover intensity
as M_Card. The overlay reads the same slot 20 and contributes nothing outside
target selection. `extend_npc_target_highlight.py` creates and assigns it.

`extend_card_target_highlight.py` installs these overrides in `M_Card`; the asset
is already saved. Re-running it is only needed when changing the material wiring.

Validation: SeaHorseEditor Development build, material compilation with D3D12,
and all eight `SeaHorse.Gameplay.Effects.*` automation tests passed. The new
`TargetOutlineTransitions` test covers candidate changes, hover changes and
restoring previous stencil/write-mask settings. In-game visual/multiplayer
inspection remains to be performed.
