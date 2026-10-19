# DLL Multiplayer — Integration guide

How to adopt the plugin and how to use each flow. Every class, function and property named here exists in
`Source/DLLMultiplayerRuntime/Public/` (verified against the headers 2026-10-19, 1.0.0). Runtime behaviour of
1.0.0 is **UNTESTED** until the matrix in [TESTING.md](TESTING.md) is run.

Contents: 1 install · 2 the subsystem · 3 hosting · 4 finding and joining · 5 state, leaving and errors ·
6 couch seats · 7 split-screen guests online · 8 quick match · 9 travel · 10 match settings · 11 invites ·
12 server browser widget · 13 existing session code · 14 debugging · 15 passwords and the player roster.

## 1. Install and pick an online subsystem

1. Clone into `Plugins/dll-multiplayer` (see the [README](../README.md#install)) and enable the plugin
   **DLL Multiplayer** (`"Name": "DLLMultiplayer"` in your `.uproject`).
2. C++: add `"DLLMultiplayerRuntime"` to your game module's `Build.cs`.
3. Choose the platform in `DefaultEngine.ini`.

**LAN only (no accounts, works offline):**

```ini
[OnlineSubsystem]
DefaultPlatformService=NULL
```

**Steam:**

```ini
[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480          ; Valve's test app id; use your own for release

[/Script/Engine.GameEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemSteam.SteamNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")
```

Steam must be running on each machine. Other platforms (EOS, consoles) are configured the engine's way; the
plugin only talks to `IOnlineSession`. At runtime, `UDLLMultiplayerSessionLibrary::GetOnlineServiceName` and
`IsUsingLAN` tell you what is active.

**Server player cap:** `MaxPlayers` lives under `[/Script/Engine.GameSession]` in `DefaultGame.ini` (with a dot;
the slash spelling is silently ignored, see [MEMORY.md](MEMORY.md)).

## 2. The subsystem

`UDLLMultiplayerSubsystem` is a `UGameInstanceSubsystem`, so there is one per game instance (one per PIE window).

```cpp
#include "DLLMultiplayerSubsystem.h"

UDLLMultiplayerSubsystem* Sessions = GetGameInstance()->GetSubsystem<UDLLMultiplayerSubsystem>();
Sessions->OnSessionCreated.AddDynamic(this, &UMyMenu::HandleSessionCreated);
Sessions->OnSessionError.AddDynamic(this, &UMyMenu::HandleSessionError);
```

Blueprint: *Get Game Instance Subsystem* (class `DLLMultiplayerSubsystem`), then *Bind Event to On Session
Created* and so on. From any world context, `UDLLMultiplayerSessionLibrary::GetSessionSubsystem(this)` does the
same and also drops stale "in session" state (§5).

Every async call reports exactly once on its delegate: `OnSessionCreated(bWasSuccessful, SessionInfo)`,
`OnSessionsFound(Sessions, bWasSuccessful)`, `OnSessionJoined(Result, SessionInfo)`,
`OnSessionDestroyed(bWasSuccessful)`, `OnSessionStarted(bWasSuccessful)`. A request that cannot start still
reports failure, so a menu never waits forever.

## 3. Hosting

| You want | Call | Notes |
|---|---|---|
| "Open my current level to friends" | `HostSessionAndTravelToLevel(LevelPath, NumConnections = 4, MatchType = "Coop", bLAN = true, bPrivate = false)` | Async. Returns true when creation **started**; the travel runs only on success, so the level gains `?listen`. Refused while already in a session. `LevelPath` is a long package name (`/Game/Levels/Hub`). |
| Your own loading screen or save before that travel | Bind `OnPreHostTravel(LevelPath)` | Runs synchronously right before the travel. Do the work and return. |
| Blueprint one-liners | `UDLLMultiplayerSessionLibrary::QuickHostSession(this, MaxPlayers, MatchType, LobbyMapPath)` / `QuickHostLANSession(this, MaxPlayers, MatchType)` / `QuickHostPrivateSession(this, MaxPlayers, MatchType)` | Return `EDLLMultiplayerSessionOperationResult`; `InProgress` means "started, wait for `OnSessionCreated`". `QuickHostSession` with a `LobbyMapPath` (e.g. `/Game/Maps/Lobby`) travels there as a listen server once the session exists; without one the host stays on the current level. The LAN and private forms stay on the current level; call `TravelToLevel` (§9) yourself. `MaxPlayers` is clamped to 1-16. |
| A private session without a data asset | `CreateQuickPrivateSession(NumConnections = 4, MatchType = "Coop", Password = "")` | Invite-only, optional password (§15). |
| C++ shortcuts | `CreateSession(NumPublicConnections, MatchType)`, `CreateLANSession(NumConnections, SessionName)` | `CreateSession` makes a LAN session when the active service is `NULL`. |
| Settings from data | `CreateSessionWithConfig(UDLLMultiplayerSessionConfiguration*)`, `CreateSessionWithSettings(FDLLMultiplayerSessionCreationSettings)`, `CreatePrivateSession(Config, Password)` | Author one `UDLLMultiplayerSessionConfiguration` data asset per lobby type (match type id, slots, visibility, join flags, lobby map, custom key/values). `FDLLMultiplayerSessionCreationSettings` adds per-host overrides. |
| A Host button that explains itself | `UDLLMultiplayerSessionLibrary::CanHostSession(this, OutReason)` | False plus a player-facing reason when hosting is impossible. A missing online service is not fatal in LAN mode. |
| Mark the match as started | `StartSession()` | Result on `OnSessionStarted`; state becomes `InProgress`. |

Creating while a session already exists leaves the old one first, then creates. Notes on data-asset fields: the
advertised build id is always the engine's `GetBuildUniqueId()` (a configured one would hide LAN sessions), and
`MaxLocalPlayers`, `bRequireAllPlayersReady`, `bAutoStartWhenReady`, `bSupportsHostMigration` and
`bPreferDedicatedServer` are stored for your game to read; the subsystem does not act on them.

## 4. Finding and joining

1. Search: `UDLLMultiplayerSessionLibrary::QuickFindSessions(this, MaxResults, bSearchLAN)`, or
   `UDLLMultiplayerSubsystem::FindSessionsFiltered(FDLLMultiplayerSessionSearchFilter)` for match type, map, game
   mode, ping, player counts and "only joinable". `FindSessions(MaxResults)` picks LAN or online from the active
   service. `RefreshSessionList()` repeats the last filter. A second search while one runs is ignored.
2. `bIncludeLAN` on the filter selects a **LAN query** (true, the default) or an **online lobby query** (false);
   it does not search both.
3. Bind `OnSessionsFound`. Build rows with `UDLLMultiplayerSessionLibrary::GetSessionListForUI(this)`
   (`FDLLMultiplayerSessionEntry`: name, host, players, ping, match type, map, `bIsJoinable`, `SessionIndex`).
   Players are people (the host's advertised headcount, §7), not machines. Style ping with
   `FormatPingForDisplay` / `GetPingColor`.
4. Join with `QuickJoinSession(this, Row.SessionIndex)` — by index, never by name — or
   `JoinSessionByInfo(SessionInfo, Password)` / `JoinSessionByIndex(Index, Password)`. The result arrives on
   `OnSessionJoined` (`EDLLMultiplayerJoinSessionResult`); `UDLLMultiplayerSessionTypeHelpers::GetJoinResultDisplayName`
   gives text. On success the client travels to the host. A password-protected session needs the password
   (`PasswordRequired` without one; `WrongPassword` if the host refuses it, §15).
5. No session at all: `DirectJoinByAddress(this, "192.168.1.20", 7777)` connects to an `ip[:port]`.

**PIE note:** PIE's "Listen Server" auto-connect is not an advertised session, so a browser will not list it.
Test hosting and browsing with two **Standalone** instances (see [TESTING.md](TESTING.md)).

## 5. State, leaving and errors

- `GetSessionState()` → `EDLLMultiplayerSessionState` (`None`, `Creating`, `Lobby`, `Starting`, `InProgress`,
  `Ending`, `Destroying`); `OnSessionStateChanged(New, Previous)` on every change.
- `IsInSession()`, `IsSessionHost()`, `GetCurrentSessionInfo()`, `GetPlayerCount()`;
  library forms `IsInSession`, `IsSessionHost`, `GetCurrentSessionName`, `GetCurrentSessionPlayerCount`.
- Leave with `UDLLMultiplayerSessionLibrary::LeaveCurrentSession(this)` or `DestroySession()`; result on
  `OnSessionDestroyed`.
- `IsInSession()` also checks that the online subsystem still has the session. If something outside the plugin
  destroyed it (for example the engine's *Destroy Session* node), `ReconcileSessionState()` resets the state to
  `None`; the session library calls it for you.
- Errors at any step arrive on `OnSessionError(ErrorCode, ErrorMessage)`. For a UI status line,
  `GetOperationResultMessage(Result)` turns an `EDLLMultiplayerSessionOperationResult` into text.

## 6. Couch seats (split-screen)

`UDLLMultiplayerLocalPlayerLibrary` works with or without a running session:

| Call | Use |
|---|---|
| `AddNextLocalPlayer(this, OutPlayerIndex, OutControllerId)` | "Press start to join": seats the next free controller. True only when a new player was created. |
| `AddLocalPlayer(this, ControllerId, OutPlayerIndex)` | Seat a specific controller. |
| `RemoveLocalPlayer(this, PlayerIndex)` | Drop a guest. Refuses seat 0. |
| `GetPlayerSlotInfo(this)` | Always exactly `GetMaxLocalPlayers()` `FDLLMultiplayerPlayerSlotInfo` entries, occupied or not — bind one lobby slot widget per entry. |
| `GetLocalPlayerCount`, `GetMaxLocalPlayers`, `FindFreeControllerId`, `IsControllerRegistered`, `GetPlayerIndexForController` | Queries for prompts and lobby UI. |

Seat limit (default 4):

```ini
; DefaultGame.ini
[/Script/DLLMultiplayerRuntime.LocalPlayers]
MaxLocalPlayers=4
```

Adding and removing seats also registers them with the subsystem (`RegisterLocalPlayer` /
`UnregisterLocalPlayer`, events `OnLocalPlayerRegistered` / `OnLocalPlayerUnregistered`). Ready-up:
`SetLocalPlayerReady(LocalPlayerIndex, bIsReady)`, `AreAllPlayersReady()`, events `OnPlayerReadyStateChanged`
and `OnAllPlayersReady`. Ready state covers this machine's seats only (see
[ARCHITECTURE.md §6](ARCHITECTURE.md#6-known-limits-current-code)). The seat limit applies to `RegisterLocalPlayer`
too; registering past it fires `OnSessionError("MaxLocalPlayers")`.

## 7. Split-screen guests online

Without this, a machine with two seats that joins online is admitted one login at a time and can be split, and on
Steam a guest leaving can unregister the host (every seat shares the machine's id).

1. GameMode: set **Game Session Class** to `DLLMultiplayerGameSession` (C++: `GameSessionClass =
   ADLLMultiplayerGameSession::StaticClass();`). Subclass it and override `OnPartyPlayerLoggedIn(NewPlayer, Info)`
   for your own per-player setup; `Info` is an `FDLLMultiplayerLoginPartyInfo`. This class also enforces session
   passwords (§15).
2. `DefaultEngine.ini`:
   ```ini
   [/Script/Engine.Engine]
   LocalPlayerClassName=/Script/DLLMultiplayerRuntime.DLLMultiplayerLocalPlayer
   ```
   Or subclass `UDLLMultiplayerLocalPlayer`, override `GetLoginPlayerKey()` to send a player id that survives
   travel, and point `LocalPlayerClassName` at your subclass.
3. Optional, `DefaultGame.ini`:
   ```ini
   [/Script/DLLMultiplayerRuntime.DLLMultiplayerGameSession]
   PartyReservationSeconds=30   ; how long guests' seats are held after the first seat is admitted (min 5)
   bAdvertiseHeadcount=true     ; write MP_HUMANS / MP_MAXHUMANS into the online session
   ```

What you get: the first seat is admitted only if the whole party fits; guests' seats are reserved; browsers and
quick match see the human headcount; guests get readable names (`"Name (2)"`); a refused party gets text from
`UDLLMultiplayerPartyRules::DescribeJoinFailure`; a guest leaving never unregisters the shared platform id.
`GetReservedSeatCount()` and `GetFreeSeatCount()` are Blueprint-readable on the game session. Debug with
`dll.Multiplayer.PartyDebug 1` on the server.

## 8. Quick match

```cpp
#include "Party/DLLMultiplayerQuickMatchSubsystem.h"

FDLLMultiplayerQuickMatchRequest Request;
Request.MatchType = TEXT("Coop");   // "" = any
Request.bLAN = false;
Request.PartySize = 0;              // 0 = every local seat on this machine
Request.MaxPing = 150;              // 0 = any
Request.bHostIfNoneFound = true;
Request.HostLevelPath = TEXT("/Game/Levels/Hub");
GetGameInstance()->GetSubsystem<UDLLMultiplayerQuickMatchSubsystem>()->StartQuickMatch(Request);
```

Bind `OnQuickMatchStateChanged(State, Message)` for progress text (`EDLLMultiplayerQuickMatchState`: `Idle`,
`Searching`, `Joining`, `Hosting`, `Succeeded`, `Failed`, `Cancelled`). `CancelQuickMatch` stops searching; a
join already travelling cannot be recalled. Ranking: games without a password that fit the whole party, fuller
first, then lower ping; a failed join tries the next (`MaxJoinAttempts`, default 3). Hosting uses
`HostSessionAndTravelToLevel` with `HostMaxConnections` (default 8). Merging separate groups into balanced
matches needs a matchmaking service and is out of scope.

## 9. Travel

Call `UDLLMultiplayerTravelLibrary::TravelToLevel(this, LevelPath, Options, bHasActiveSession)` **on the server**
for every party-wide level change. `LevelPath` is a long package name, not an object path; `Options` is appended
(for example `?game=/Script/MyGame.MyGameMode`). It returns the `EDLLMultiplayerTravelMethod` used:

| Situation | Method |
|---|---|
| Dedicated or listen server, or an active session | `ServerTravel` (adds `?listen` unless dedicated) |
| Standalone, no session | `OpenLevel` |
| Called on a client | `None` (refused — clients follow the server) |

With non-seamless travel every client reconnects on each change, and PIE clients do not reconnect (they fall
back to `GameDefaultMap`). For multi-level co-op use seamless travel with a `TransitionMap`.

## 10. Match settings

`FDLLMultiplayerMatchSettings` holds lobby rules: session type, map and game mode, score/time/round/stock limits,
friendly fire, pickups, respawn and spawn protection, teams (`FDLLMultiplayerTeamConfig`), horde waves and
difficulty, mutator tags, and a `CustomSettings` string map for anything game-specific. `SetupDefaultTeams()`
creates two teams; their `SpawnTag` is unset — set your own tag if your spawn logic needs one.

- The host changes them with `UDLLMultiplayerSubsystem::UpdateMatchSettings(NewSettings)` while the session is in
  `Lobby`. They are validated, then serialized into the online session under the key `MatchSettings`.
- Anyone reads the host's copy with `GetCurrentMatchSettings()`. `FDLLMultiplayerMatchSettings::DeserializeFromString`
  rebuilds them from an advertised string.
- `UDLLMultiplayerMatchSettingsPreset` is a data asset wrapping a named settings set for a lobby picker.

The plugin stores and advertises these values; your game mode applies them.

## 11. Invites

Invites go through the online service (Steam, EOS, ...). The Null (LAN) subsystem has no invites.

- **Send:** `SendInvite(FriendId)` while in a session. The result arrives on `OnInviteSent(bWasSuccessful,
  RecipientId)`; a service that cannot send also fires `OnSessionError("InviteFailed")`.
- **Receive:** `OnGameInviteReceived(Invite)` fires when an invite arrives. `Invite.FromPlayer.DisplayName` holds
  the sender's id; look up a friendly name with the friends interface if you need one. Answer with
  `AcceptInvite(Invite)` or `DeclineInvite(Invite)`.
- **Accepted from the platform** (Steam overlay "Join Game", `+connect_lobby` launch): handled automatically. If
  the player is in a session it leaves first, then joins; the result arrives on `OnSessionJoined`.
- `UDLLMultiplayerSessionLibrary::OpenSteamOverlay()` opens the platform friends overlay for inviting.
- Invites into password-protected sessions are refused by the host (an invite cannot carry the password).

## 12. Server browser widget

Create a Widget Blueprint whose parent is `DLLMultiplayerSessionBrowserWidget`:

1. Optionally add widgets named `RefreshButton`, `JoinButton`, `BackButton`, `SessionCountText`, `StatusText`,
   `PasswordInput`; they are bound automatically when present.
2. Call `SetupBrowser(Filter)` or `SetupBrowserDefaults()` (LAN query, 100 results) when the screen opens.
3. Implement the events `OnSessionListUpdated`, `OnSessionSelected`, `OnJoiningSession`,
   `OnJoinSessionComplete`, `OnBrowserError` to draw your list.
4. Use `SelectSessionByIndex`, `JoinSelectedSession(Password)`, `SortSessions(Mode, bAscending)`,
   `ApplyFilter`, `SetMatchTypeFilter`.

## 13. If your game already has session code: forward, don't move

Keep your game's classes at their current class paths with unchanged `UFUNCTION` signatures and make their
bodies one-line forwards to the plugin. Blueprint nodes and parent classes resolve by class path; moving a class
(for example your GameInstance) orphans assets unless a `CoreRedirect` is exactly right, and a wrong redirect
stops the project booting. Copy plugin structs field by field into your existing UI structs at the boundary, and
map result enums with an explicit `switch` (never `static_cast`).

## 14. Debugging

| Tool | What it shows |
|---|---|
| `bEnableVisualDebug`, `bDebugShowSessionState`, `bDebugShowPlayerList` on `UDLLMultiplayerSubsystem` (*DLL Multiplayer → Debug*) | Service, session state, session and players on screen. The plugin does not tick: call `DrawDebugInfo()` from your own HUD or debug tick while you need it. |
| `dll.Multiplayer.PartyDebug 1` | Party admission, reservations and shared ids on the server, redrawn at every login and logout |
| `OnSessionError` / `GetOperationResultMessage` / `DescribeJoinFailure` | Player-readable reason for any failed step |
| Output log (`LogDLLMultiplayerParty`, `LogDLLMultiplayerQuickMatch`, `[DLLMultiplayer Travel]`, `[DLLMultiplayer LocalPlayers]`, `[DLLMultiplayer]`, `DLLMultiplayer:`) | One line per operation; every ServerTravel logs its URL and net mode |

## 15. Passwords and the player roster

**Passwords.** Pass a password to `CreatePrivateSession(Config, Password)`, `CreateQuickPrivateSession(...,
Password)` or `FDLLMultiplayerSessionCreationSettings`. It may not contain spaces or `? = # &` (it travels in the
connect URL); creation refuses such a password with `OnSessionError("InvalidPassword")`. Only the fact that a
password exists is advertised (`FDLLMultiplayerSessionInfo::bHasPassword`).

1. The host's GameMode must use `ADLLMultiplayerGameSession` (or a subclass): its `ApproveLogin` does the check.
   With a game session class of your own, call `UDLLMultiplayerSubsystem::IsSessionPasswordAccepted(
   UGameplayStatics::ParseOption(Options, TEXT("Password")))` from your `ApproveLogin` and return
   `UDLLMultiplayerSubsystem::ErrorWrongPassword` when it is false.
2. The client joins with `JoinSessionByInfo(Info, Password)`, `JoinSessionByIndex(Index, Password)` or the browser
   widget's `JoinSelectedSession(Password)`. Without a password the join is refused up front with
   `PasswordRequired`.
3. `OnSessionJoined(Success)` fires when the online join completes and the client starts connecting. If the host
   then refuses the password, `OnSessionJoined(WrongPassword)` follows and the client leaves the session. Show
   "connecting" on `Success` and the final result on `WrongPassword` or the first level load.

Quick match skips password-protected games, and invited players cannot join one (an invite carries no password).

**Roster (host only).** While this machine hosts, `GetSessionPlayers()` lists every player (local seats and
remote players), and:

| Event | When |
|---|---|
| `OnPlayerJoined(Player, bIsLocalPlayer)` | A player logs in for the first time. A re-login after a level change is not announced again. |
| `OnPlayerLeft(Player, Reason)` | `Voluntary` (logged out), `Kicked` (removed with `KickPlayer`), `ConnectionLost` (did not return within 30 seconds of a map change) |

`KickPlayer(PlayerId)` (host only) removes a remote player through the game session; when it cannot, it returns
false and fires `OnSessionError` (`NotHost` or `KickFailed`). Clients do not receive these events; build a
client-side player list from replicated PlayerStates.
