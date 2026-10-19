# DLL Multiplayer — Roadmap

## 1.0.0 — First public release (in progress)
- ✅ Session layer: online / LAN / private sessions, filtered find, join, leave, start, state machine, Blueprint
  delegates, `HostSessionAndTravelToLevel`.
- ✅ Couch seats, party-wide travel, split-screen guests online, party-aware quick match, match settings, server
  browser widget base.
- ✅ Host-enforced session passwords; host-side roster (`OnPlayerJoined` / `OnPlayerLeft`, `KickPlayer`); invites
  through the online service (`SendInvite`, `OnGameInviteReceived`); `CreateQuickPrivateSession`;
  `QuickHostSession` with a `LobbyMapPath`.
- ✅ Standalone `BuildPlugin` on the 1.0.0 source (2026-10-19).
- ⬜ Adopter verification: the reference adopter builds on 1.0.0 and passes the host/join matrix in
  [TESTING.md](TESTING.md) (LAN, Steam, split-screen guests, travel, passwords, roster).
- ⬜ Public release: publish the repository, tag `v1.0.0`.

## 1.1 — Tests and polish
- ⬜ Pure-logic automation tests in the plugin as `DLLMultiplayer.*` (party rules, seat limit, quick match ranking) (G3).
- ⬜ Settings: `UDLLMultiplayerSettings` (Project Settings → Plugins → DLL Multiplayer) for the seat limit and
  party options.
- ⬜ Replicated ready state for remote players, so `AreAllPlayersReady` covers the whole session.

## Later
- Client-side roster.
- Optional modules: voice (`DLLMultiplayerVoice`), EOS specifics.
- Seamless-travel support helpers (transition map, `CopyProperties` guidance).
