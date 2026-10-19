# DLL Multiplayer — Architecture

## 1. Layers

```
Adopting game ── calls / forwards to ──▶ Blueprint libraries
                                           UDLLMultiplayerSessionLibrary      (host / find / join / leave, browser rows)
                                           UDLLMultiplayerLocalPlayerLibrary  (couch seats)
                                           UDLLMultiplayerTravelLibrary       (party-wide travel)
                                                 │
                                                 ▼
                     UDLLMultiplayerSubsystem (GameInstance)  ◀── UDLLMultiplayerQuickMatchSubsystem (GameInstance)
                                                 │               ◀── UDLLMultiplayerSessionBrowserWidget (UMG base)
                                                 ▼
                     IOnlineSession of this world's online subsystem (NULL/LAN, Steam, EOS, ...)

Server side of a join:  ADLLMultiplayerGameSession  ◀── login options ──  UDLLMultiplayerLocalPlayer (each seat, each travel)
                              └── UDLLMultiplayerPartyRules (pure rules, no world access)
```

One runtime module, `DLLMultiplayerRuntime` (plugin `DLLMultiplayer`). It never references a game module. Games
either call it directly or keep their own facades with unchanged signatures that forward here
([INTEGRATION_GUIDE.md §13](INTEGRATION_GUIDE.md#13-if-your-game-already-has-session-code-forward-dont-move)).

## 2. Public surface

| Class | Role | Key API |
|---|---|---|
| `UDLLMultiplayerSubsystem` | Session lifecycle + state | `CreateSession`, `CreateSessionWithConfig`, `CreateSessionWithSettings`, `CreatePrivateSession`, `CreateQuickPrivateSession`, `CreateLANSession`, `FindSessions`, `FindSessionsFiltered`, `RefreshSessionList`, `GetCachedSessionList`, `JoinSessionByInfo`, `JoinSessionByIndex`, `DestroySession`, `StartSession`, `HostSessionAndTravelToLevel`, `GetSessionState`, `IsInSession`, `IsSessionHost`, `ReconcileSessionState`, `GetCurrentSessionInfo`, `GetCurrentMatchSettings` / `UpdateMatchSettings`, `RegisterLocalPlayer` / `UnregisterLocalPlayer`, `GetSessionPlayers`, `GetPlayerCount`, `SetLocalPlayerReady`, `AreAllPlayersReady`, `SendInvite` / `AcceptInvite` / `DeclineInvite`, `KickPlayer`, `GetCurrentServiceType`, `GetServiceName`, `IsUsingLAN`; C++ `IsSessionPasswordAccepted` (host password check). Delegates: `OnSessionCreated`, `OnSessionsFound`, `OnSessionJoined`, `OnSessionDestroyed`, `OnSessionStarted`, `OnSessionStateChanged`, `OnPreHostTravel`, `OnPlayerJoined` / `OnPlayerLeft` (host), `OnPlayerReadyStateChanged`, `OnAllPlayersReady`, `OnGameInviteReceived`, `OnInviteSent`, `OnSessionError`, `OnLocalPlayerRegistered` / `Unregistered`. |
| `UDLLMultiplayerSessionLibrary` | Blueprint helpers | `GetSessionSubsystem`, `IsSessionSystemAvailable`, `QuickHostSession`, `QuickHostLANSession`, `QuickHostPrivateSession`, `QuickFindSessions`, `QuickJoinSession`, `DirectJoinByAddress`, `LeaveCurrentSession`, `GetSessionListForUI`, `CanHostSession`, `GetOperationResultMessage`, `FormatPingForDisplay`, `GetPingColor`, `OpenSteamOverlay`. Returns `EDLLMultiplayerSessionOperationResult`. |
| `UDLLMultiplayerLocalPlayerLibrary` | Couch seats | `GetLocalPlayerCount`, `GetMaxLocalPlayers` (ini), `GetPlayerSlotInfo` → `FDLLMultiplayerPlayerSlotInfo`, `AddLocalPlayer`, `AddNextLocalPlayer`, `FindFreeControllerId`, `RemoveLocalPlayer`, `IsControllerRegistered`, `GetPlayerIndexForController` |
| `UDLLMultiplayerTravelLibrary` | Travel | `TravelToLevel(WorldContext, LevelPath, Options, bHasActiveSession)` → `EDLLMultiplayerTravelMethod` |
| `ADLLMultiplayerGameSession` | Server admission | `ApproveLogin` checks the session password and judges the whole party; seat reservations (`PartyReservationSeconds`); shared-id-safe `UnregisterPlayer`; advertises `MP_HUMANS` / `MP_MAXHUMANS` (`bAdvertiseHeadcount`); `GetReservedSeatCount`, `GetFreeSeatCount`; `OnPartyPlayerLoggedIn` hook |
| `UDLLMultiplayerLocalPlayer` | Client identity | `GetGameLoginOptions` (party id / size / seat / key), per-seat `GetNickname`, `GetSeatIndex`, `GetMachinePartyId`, `GetLoginPlayerKey` hook |
| `UDLLMultiplayerPartyRules` | Pure rules | `BuildLoginOptions`, `ParseLoginOptions` → `FDLLMultiplayerLoginPartyInfo`, `GetFreeSeats`, `CanPartyFit`, `EvaluateJoin`, `MakeGuestNickname`, `DescribeJoinFailure`, `IsUniqueIdSharedWithAnotherPlayer`, `RankQuickMatchCandidates`, `GetPartyInfoForController` |
| `UDLLMultiplayerQuickMatchSubsystem` | Quick match | `StartQuickMatch(FDLLMultiplayerQuickMatchRequest)`, `CancelQuickMatch`, `GetState` → `EDLLMultiplayerQuickMatchState`, `IsRunning`, `OnQuickMatchStateChanged` |
| `UDLLMultiplayerSessionConfiguration` | Data asset | Identity, type, visibility, player limits, join flags, lobby map, allowed maps, custom key/values, tags; `ValidateConfiguration`, `IsMapAllowed`, `GetLobbyMapPathWithOptions` |
| `FDLLMultiplayerSessionCreationSettings` | Struct | A configuration plus per-host overrides (name, map, password, visibility, slots, extra key/values) |
| `FDLLMultiplayerMatchSettings`, `FDLLMultiplayerTeamConfig`, `UDLLMultiplayerMatchSettingsPreset` | Match rules | Win conditions, generic rules, teams, horde waves, mutator tags, custom key/values; preset data asset |
| `UDLLMultiplayerSessionBrowserWidget` | UI base | Abstract UMG base: `SetupBrowser`, `RefreshSessionList`, `ApplyFilter`, `SortSessions`, select/join, optional bound buttons/text, Blueprint events |
| `UDLLMultiplayerSessionTypeHelpers` | Blueprint helpers | Display names for session type / visibility / state / join result; per-type defaults |

Types: `FDLLMultiplayerSessionInfo` (browser and current session, built by `FromSearchResult`),
`FDLLMultiplayerSessionPlayer`, `FDLLMultiplayerSessionSearchFilter`, `FDLLMultiplayerGameInvite`,
`FDLLMultiplayerSessionHandle`, `FDLLMultiplayerSessionEntry` (browser row); enums
`EDLLMultiplayerSessionType`, `EDLLMultiplayerSessionVisibility`, `EDLLMultiplayerSessionState`,
`EDLLMultiplayerJoinSessionResult`, `EDLLMultiplayerPlayerLeaveReason`, `EDLLMultiplayerOnlineServiceType`,
`EDLLMultiplayerSessionOperationResult`, `EDLLMultiplayerSessionSortMode`.

**Internal (not API):** the subsystem's operation tracker (`EDLLMultiplayerSessionOpKind`,
`FDLLMultiplayerSessionOp`, `TrackOp` / `FinishOp`), the leave-then continuation (`LeaveThen`), the single
settings builder (`BuildOnlineSettings`), and the `DLLMultiplayerSessionCore` helpers in the `.cpp`. The
`OnlineSubsystem` completion handlers are `protected` for subclasses but are not a contract.

## 3. Key flows

**Session operations.** Each async call (create, find, join, destroy, start) binds its online-service completion
when it starts and unbinds it when it ends, so nothing listens while idle and at most one of each kind runs.
Every request reports its result exactly once on its delegate, including synchronous refusals (the Null service
can fire a completion before its call returns false; `FinishOp` prevents a double report). Creating while a
session exists leaves it first, then creates (`LeaveThen`); an accepted platform invite outranks a queued create.

**Stale state.** `IsInSession` also asks the online subsystem whether the session still exists. A session
destroyed outside the plugin (for example by the engine's *Destroy Session* node) is detected and
`ReconcileSessionState` resets the cached state; the session library calls it before each operation.

**Host and travel.** `HostSessionAndTravelToLevel` → create (LAN, private or online) → on success broadcast
`OnPreHostTravel(LevelPath)` → `UDLLMultiplayerTravelLibrary::TravelToLevel` (the session is active, so it is a
ServerTravel even from Standalone, with `?listen` re-stated). Refused while already in a session.

**Travel routing.** Dedicated or listen server, or an active session → `ServerTravel` (with `?listen` unless
dedicated). Standalone, no session → `OpenLevel`. Client → refused (`EDLLMultiplayerTravelMethod::None`). Every
ServerTravel logs the URL, net mode and session flag (`[DLLMultiplayer Travel]` log prefix).

**Split-screen guests online.** Each seat's `UDLLMultiplayerLocalPlayer::GetGameLoginOptions` carries the
machine's party id, party size and seat index (URL options `MPParty`, `MPPartySize`, `MPSeat`, optional `MPKey`)
on the primary login and every split join, re-sent after every non-seamless travel.
`ADLLMultiplayerGameSession::ApproveLogin` admits the first seat only if the whole party fits and reserves the
guests' seats for `PartyReservationSeconds`; guests consume their reservation in `PostLogin`; a primary seat
logging out drops its party's reservation. The engine-written `SplitscreenCount` option is the trusted guest
signal; refusals return the tokens `MPParty.Full`, `MPParty.NoRoom…`, `MPParty.TooManyLocal`, which
`DescribeJoinFailure` turns into player text.

**Headcount.** On every login and logout the game session writes `MP_HUMANS` / `MP_MAXHUMANS` into the hosted
online session (skipped when unchanged). `FDLLMultiplayerSessionInfo::FromSearchResult` reads them back, so a
browser and quick match count people, not machines.

**Quick match.** `FindSessionsFiltered` → `UDLLMultiplayerPartyRules::RankQuickMatchCandidates` (no password,
room for the whole party, under `MaxPing`; fuller first, then ping) → join the best → next candidate on a failed
join (up to `MaxJoinAttempts`) → `HostSessionAndTravelToLevel(HostLevelPath)` if none fit and
`bHostIfNoneFound`. The host re-checks room at login, so a game that filled in the meantime refuses the party
instead of splitting it.

**Passwords.** The host keeps the password and advertises only `HasPassword`. `JoinSessionByInfo` refuses a
password-protected session without a password (`PasswordRequired`); otherwise, after the online join succeeds,
the client travels to the host with `?Password=` on the URL. `ADLLMultiplayerGameSession::ApproveLogin` checks a
machine's first login (`SplitscreenCount < 2`) with `UDLLMultiplayerSubsystem::IsSessionPasswordAccepted` and
refuses with `WrongPassword`; the client's subsystem sees that network failure, reports
`OnSessionJoined(WrongPassword)` and leaves the online session. Passwords may not contain spaces or `? = # &`
(they travel in the URL); creation refuses them with `OnSessionError("InvalidPassword")`.

**Roster (host only).** While hosting, the subsystem listens to the GameMode's PostLogin / Logout events and keeps
`GetSessionPlayers` current, firing `OnPlayerJoined(Player, bIsLocalPlayer)` and `OnPlayerLeft(Player, Reason)`.
A re-login after a level change refreshes the entry instead of announcing the player again. Logouts during a map
change are ignored; 30 seconds after the last one, players who did not return are removed with
`ConnectionLost`. `KickPlayer` removes a remote player through the game session; the resulting leave reports
`Kicked`, any other logout `Voluntary`.

**Platform invites.** `Initialize` binds the online session's invite-received and invite-accepted events.
A received invite fires `OnGameInviteReceived` (the sender's display name is their id; resolve it with the
friends interface if you need a name). Accepting (Steam overlay "Join Game", `+connect_lobby` launches,
`AcceptInvite`) leaves the current session first, then joins. `SendInvite` asks the online service to send a
session invite and reports the result on `OnInviteSent`.

## 4. Extension points

- `UDLLMultiplayerSubsystem::OnPreHostTravel` — loading screens, saves, transition hooks before host travel.
- `ADLLMultiplayerGameSession::OnPartyPlayerLoggedIn` (virtual) — stamp game identity on a new seat.
- `UDLLMultiplayerLocalPlayer::GetLoginPlayerKey` (virtual) — send a travel-stable player key with the login.
- Subclassing `UDLLMultiplayerSessionBrowserWidget` in UMG; `BindWidgetOptional` slots for common controls.
- Session configuration and match-settings data assets; `CustomSettings` maps on both for game-specific values.
- `FDLLMultiplayerTeamConfig::SpawnTag` — unset by default; the game supplies its own tag.

Prefer adding an extension point over a game branch.

## 5. Debug

| Tool | Where | Shows |
|---|---|---|
| `bEnableVisualDebug`, `bDebugShowSessionState`, `bDebugShowPlayerList` | `UDLLMultiplayerSubsystem`, category *DLL Multiplayer → Debug* | Service, state, session, players on screen. Drawn by `DrawDebugInfo()`, which the plugin does not call itself (no Tick); call it from your HUD. |
| `dll.Multiplayer.PartyDebug 1` (cheat CVar) | `ADLLMultiplayerGameSession`, server | Seat / reservation / shared-id table, redrawn on every login and logout (event-driven). |
| Log categories `LogDLLMultiplayerParty`, `LogDLLMultiplayerQuickMatch`; `LogTemp` lines prefixed `DLLMultiplayer:`, `[DLLMultiplayer]`, `[DLLMultiplayer Travel]`, `[DLLMultiplayer LocalPlayers]` | Output log | One line per operation, never per frame. |

## 6. Known limits (current code)

- The roster (`GetSessionPlayers`, `OnPlayerJoined`, `OnPlayerLeft`) is **host-only**. Clients see only their own
  seats; use replicated PlayerStates for a client-side player list.
- Ready state covers this machine's seats only: `SetLocalPlayerReady` sets local seats, and remote players' ready
  flags are not replicated, so `AreAllPlayersReady` stays false on the host once a remote player is in the roster.
- Invites into password-protected sessions are refused: an invite cannot carry the password, so the host rejects
  the invited player with `WrongPassword`.
- A join reports `OnSessionJoined(Success)` when the online join completes and may then report
  `OnSessionJoined(WrongPassword)` once the host refuses the login. A UI should treat `Success` as "connecting".
- The password check runs in `ADLLMultiplayerGameSession`. A GameMode with a different game session class must
  call `IsSessionPasswordAccepted` from its own `ApproveLogin`, or passwords are not enforced.
- `FDLLMultiplayerSessionSearchFilter::bIncludeLAN` means **LAN-only query** when true (the default) and online
  lobby query when false; there is no combined search.
- `SendInvite` depends on the online service: the Null (LAN) subsystem has no invites, so it reports failure.
- `QuickHostPrivateSession` has no password parameter; use `CreateQuickPrivateSession` or `CreatePrivateSession`.

## 7. Module plan

Single runtime module today (`DLLMultiplayerRuntime`). Candidates for optional modules: voice
(`DLLMultiplayerVoice`), EOS-specific features. See [ROADMAP.md](ROADMAP.md).
