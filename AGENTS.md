\# SeaHorse coding rules



\- Unreal Engine multiplayer card game.

\- Server authoritative gameplay.

\- Gameplay state must not live in widgets.

\- GameMode is server-only.

\- Replicated shared state belongs in GameState.

\- Player-specific replicated state belongs in PlayerState.

\- Prefer C++ base classes that can be subclassed in Blueprint.

\- Do not modify Blueprint assets unless explicitly requested.

