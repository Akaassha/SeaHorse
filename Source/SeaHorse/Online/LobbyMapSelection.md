# Lobby map selection

Configure **Project Settings > Game > SeaHorse Multiplayer > Maps**:

- **Small Match Map (4 seats)**: `L_Test` by default. The config key remains `MatchMap` for compatibility.
- **Medium Match Map (6 seats)**: `L_Game_SixPlayers` by default.
- **Default Match Map**: initial choice when hosting (Small by default).

Both levels are included in `Config/DefaultGame.ini` under `MapsToCook`. If replacing them with other levels, update the packaging list too. Each level must use the gameplay GameMode/deck and contain unique consecutive `BP_Hand.LayoutSeatIndex` values (0–3 or 0–5), with its usual representations and victory stacks. Unoccupied seats become BN stacks.

## Widget wiring, to be done later

No widget assets are changed by this implementation.

1. Get the owning `SHLobbyPlayerController` and call **Server Select Match Map** with `ESHMatchMap.Small` or `Medium`.
2. Use **Is Lobby Host** on that controller to show the arrows only to the host (Visible for the host, Collapsed for guests). Keep host arrows enabled. Use **Can Start Match** on GameState to enable Start; it includes selected-map capacity.
3. Read **Get Lobby Info** on `SHLobbyGameState`: `SelectedMap`, `MaxPlayers`, `bChangingMap`, `bStartingMatch`.
4. Refresh the display on **On Lobby Changed**, and once immediately when constructing the lobby UI. This also covers late joiners.
5. Show rejection messages from the controller's **On Lobby Request Rejected** event.
6. Existing **Server Start Match** and readiness controls remain the same.

The server accepts only the host's request. Selecting the current map is a no-op. A successful change resets readiness for guests, preserving the host's current readiness. The old map and readiness remain if the service rejects the update. During the asynchronous session update, start/readiness/map changes and new admissions are blocked; connection attempts may retry afterwards. Switching to Small is allowed with more than four humans; Start remains blocked until the roster fits or the host chooses Medium.

`CreateMatch` accepts 2–6 human slots. Its existing `MaxPlayers` argument remains the initial room limit; a 5–6 slot room automatically starts with Medium if the default was Small. Explicitly switching map in the lobby expands the room's public slots when needed (up to 6), including the online session advertisement. Selecting Small never shrinks the lobby; the 4-seat constraint applies to starting that map. Selecting Medium as the project default does not itself override the `MaxPlayers` argument passed by the existing widget.

At Start, the server snapshots the confirmed map and builds its travel URL with `SHExpectedPlayers` set to the actual human roster, not the seat capacity. Clients follow the existing seamless server travel. The destination level retains its World Settings GameMode and deck configuration.

Automated checks: `SeaHorse.Multiplayer` (including `LobbyMapSelection`). Manual Steam/LAN verification after UI hookup: two clients see the same choice, guests cannot change it, successful switches reset guest readiness and update available slots, six humans can switch to Small but cannot start it, and Start loads the selected level for everyone.
