# Changelog

All notable changes to DLL Multiplayer. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versioning: [SemVer](https://semver.org/).

## [1.0.0] - Unreleased

First public release. Standalone `BuildPlugin` passes (2026-10-19, UE 5.7, Win64); host/join tests in the reference adopter are **UNTESTED** ([docs/TESTING.md](docs/TESTING.md)).

### Added
- **`UDLLMultiplayerSubsystem`** (GameInstance subsystem): create online / LAN / private sessions from plain
  arguments, a `UDLLMultiplayerSessionConfiguration` data asset or `FDLLMultiplayerSessionCreationSettings`;
  filtered find with a cached result list; join by info or index; leave; start; a session state machine with
  stale-state reconciliation; one Blueprint delegate per step (`OnSessionCreated`, `OnSessionsFound`,
  `OnSessionJoined`, `OnSessionDestroyed`, `OnSessionStarted`, `OnSessionStateChanged`, `OnSessionError`). Every
  request reports its result exactly once, including requests that cannot start.
- **Host the level you are in:** `HostSessionAndTravelToLevel` creates the session first and only then reopens
  the level as a listen server; `OnPreHostTravel` lets a game run its own loading screen or save first.
- **Password-protected sessions** enforced by the host: the client sends `?Password=` with its login,
  `ADLLMultiplayerGameSession::ApproveLogin` checks it through `UDLLMultiplayerSubsystem::IsSessionPasswordAccepted`,
  and a refused client receives `OnSessionJoined(WrongPassword)` and leaves the session. Only the fact that a
  password exists is advertised. Passwords may not contain spaces or `? = # &`.
- **Host-side roster:** `OnPlayerJoined` / `OnPlayerLeft` (reasons `Voluntary`, `Kicked`, and `ConnectionLost`
  for players who do not return after a map change), `GetSessionPlayers`, `KickPlayer`.
- **Invites:** `SendInvite` through the online service, `OnInviteSent`, `OnGameInviteReceived`, and automatic
  handling of accepted platform invites (leave the current session, then join).
- `CreateQuickPrivateSession` (private session without a data asset); ready state for local seats
  (`SetLocalPlayerReady`, `AreAllPlayersReady`, `OnAllPlayersReady`); local-player registry.
- **`UDLLMultiplayerSessionLibrary`:** Blueprint one-liners — `QuickHostSession` (optionally travelling to a
  `LobbyMapPath`), `QuickHostLANSession`, `QuickHostPrivateSession`, `QuickFindSessions`, `QuickJoinSession`,
  `DirectJoinByAddress`, `LeaveCurrentSession`, browser rows (`GetSessionListForUI`), `CanHostSession`, ping text
  and colour, readable result messages.
- **`UDLLMultiplayerLocalPlayerLibrary`:** couch seats — add/remove local players, "press start to join", per-seat
  slot info, controller → seat, seat limit from `[/Script/DLLMultiplayerRuntime.LocalPlayers] MaxLocalPlayers`.
- **`UDLLMultiplayerTravelLibrary::TravelToLevel`:** party-wide travel routed on net mode, `?listen` re-stated on
  every non-dedicated ServerTravel, refused on clients.
- **Split-screen guests online:** `ADLLMultiplayerGameSession`, `UDLLMultiplayerLocalPlayer` and
  `UDLLMultiplayerPartyRules` — whole-party admission with seat reservations, shared-platform-id-safe unregister,
  advertised human headcount, per-seat guest names; console variable `dll.Multiplayer.PartyDebug`.
- **`UDLLMultiplayerQuickMatchSubsystem`:** party-aware quick match (games that fit the whole party, fuller first,
  then ping; next candidate on a failed join; host one if none fit).
- **Match settings:** `FDLLMultiplayerMatchSettings`, `FDLLMultiplayerTeamConfig`,
  `UDLLMultiplayerMatchSettingsPreset`, advertised with the session.
- **`UDLLMultiplayerSessionBrowserWidget`:** abstract UMG base for a server browser.
- Visual debug toggles on the subsystem; log categories `LogDLLMultiplayerParty`, `LogDLLMultiplayerQuickMatch`.
- Docs: README, `NOTICE` and the `.uplugin` `CreatedByURL` link [digilogiclabs.com](https://digilogiclabs.com); the
  README license section explains Apache-2.0 in plain words (free for any use, including commercial; keep
  LICENSE/NOTICE).
- Community files: `CODE_OF_CONDUCT.md` (Contributor Covenant 2.1), `SECURITY.md` (private reporting), issue-template
  `config.yml`, and fork → build → test → pull request steps in `CONTRIBUTING.md`.
