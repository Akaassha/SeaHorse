# Steam avatars in Blueprint

`Load Steam Avatar Async` accepts an `APlayerState` (lobby and match PlayerStates
both work). `On Success` supplies a transient `Texture`; `On Failure` supplies
an `Error` and a null texture. The node can finish immediately for cached images.

In `WBP_LobbyEntry`:

1. Add an Image with a default avatar brush.
2. Add a PlayerState object reference, Instance Editable and Expose on Spawn.
3. In the lobby's player loop, pass Array Element to this new Create Widget pin.
4. On Construct, call Load Steam Avatar Async with this PlayerState.
5. On Success, Set Brush from Texture on the Image (Match Size false).
6. On Failure, keep the default brush. Do not retry every frame.

Keep the original PlayerState even if the displayed nickname is shortened.
Never resolve identity by nickname. For reused rows, verify that the current
PlayerState still matches the request before applying its result. Existing lobby
rows are recreated on lobby changes and receive their own completion callbacks.

The local GameInstance subsystem caches up to 256 successful 128px textures.
Every client downloads directly from Steam; images are not replicated. The
current Steam avatar is cached for this GameInstance; profile changes during
the same game run are not actively refreshed. A new row gets a cached texture
without rebuilding it. Pending SDK downloads are polled every 0.1 seconds, using
the Steam callbacks already processed by OnlineSubsystemSteam.

The request waits up to 20 seconds for initial PlayerState identity replication
and image availability. Missing avatars also expire through this timeout, so
the default image stays visible. Offline/NULL service, invalid players and
unsupported platforms fail gracefully. Supported platforms: Win64, Linux, Mac.

The async node is not a Delay: Cancel Latent Actions on a widget does not cancel
it. Requests finish on success, failure, timeout or GameInstance shutdown.
Do not have their callbacks reopen screens or mutate gameplay state.

Manual Steam verification: two accounts in a lobby, both nicknames and avatars;
toggle readiness repeatedly and verify cached images; leave while downloading;
test an account without an avatar and an offline launch. Automated lifecycle
coverage: `SeaHorse.Avatars.RequestLifecycle`.
