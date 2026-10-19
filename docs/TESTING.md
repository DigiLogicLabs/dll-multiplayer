# DLL Multiplayer — Testing

**Rule:** never claim "compiles" or "tested" without an artifact (build log line, test report, PIE note).
`UNTESTED` is a valid, honest status.

## 1. Standalone build

```powershell
& "<UE>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  "-Plugin=<repo>\DLLMultiplayer.uplugin" "-Package=D:\tmp\dllmp_pkg" -TargetPlatforms=Win64 -Rocket
```

Pass = `BUILD SUCCESSFUL` for both the editor and the game target. Proves no hidden dependency on any game. Use a
short `-Package` path (Windows 260-character limit).

| Source | Date | Result |
|---|---|---|
| Pre-release source, before the password / roster / invite additions | 2026-10-18, UE 5.7 | **PASS** — `BUILD SUCCESSFUL`, no warnings from plugin sources |
| Final 1.0.0 source | 2026-10-19, UE 5.7 | **PASS** — `BUILD SUCCESSFUL`, no warnings from plugin sources |

## 2. Automation tests

The plugin does not ship automation tests yet (roadmap G3 adds them as `DLLMultiplayer.<Area>.<Behaviour>`,
run with `Automation RunTests DLLMultiplayer`). Until then the pure-rule tests live in the reference adopter
(prefixes `<Adopter>.Party.*` and `<Adopter>.Multiplayer.*`):

| Adopter test | Covers |
|---|---|
| `Party.LoginOptions` | `UDLLMultiplayerPartyRules` build/parse round-trip |
| `Party.Admission` | Whole-party admission, reservations |
| `Party.QuickMatchRanking` | `RankQuickMatchCandidates` |
| `Party.NamesAndFailureText` | Guest nicknames, `DescribeJoinFailure` |
| `Multiplayer.ResultEnumParity` / `ResultMapping` | Adopter↔plugin result enum mapping (adopter bridge) |
| `Multiplayer.PresentationForwarding` | Adopter facade forwards to the plugin |
| `Multiplayer.LocalPlayerLimit` | `GetMaxLocalPlayers` ini read + clamp |

```powershell
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests <Adopter>.Party+<Adopter>.Multiplayer; Quit" -unattended -nullrhi -nosplash
```

Status on 1.0.0: **UNTESTED**.

## 3. Adopter build check (1.0.0)

| Case | How | Pass | Status |
|---|---|---|---|
| Adopter builds on 1.0.0 | Adopter `Build.cs` depends on `DLLMultiplayerRuntime`; full build with the editor closed | `Result: Succeeded` | **UNTESTED** |
| Blueprints compile | Open the editor; compile the Blueprints and widgets that use the plugin | No missing class/struct/enum warnings | **UNTESTED** |
| Seat limit | `[/Script/DLLMultiplayerRuntime.LocalPlayers] MaxLocalPlayers=2` in `DefaultGame.ini` | `GetMaxLocalPlayers` returns 2; a third seat is refused | **UNTESTED** |

## 4. Manual matrix (all **UNTESTED** on 1.0.0)

| Case | How | Pass |
|---|---|---|
| LAN host/browse/join | Two **Standalone** instances (PIE "Listen Server" auto-connect is not an advertised session) | Browser lists host; join travels |
| Steam host/join | Two machines, Steam running | Same; browser shows humans/max |
| Split-screen guest online | Client machine with 2 seats joins a host | Both seats admitted together; full host refuses party with readable text |
| Guest leaves | Guest seat drops | Owner stays registered in the session |
| Travel after join | Host calls `TravelToLevel` | Clients follow; host still listening (`?listen` in the logged `[DLLMultiplayer Travel]` URL) |
| Host the level I'm in | `HostSessionAndTravelToLevel` from a standalone level | Level reloads as listen server; a second instance can find and join it |
| Quick host with lobby map | `QuickHostSession` with `LobbyMapPath` set | Host travels to the lobby map as a listen server once the session exists |
| Quick match | Party of 2, one near-full game, one roomy | Picks roomy; falls through on failed join |
| Password: right | Host `CreateQuickPrivateSession(4, "Coop", "abc")`; client joins with `abc` | Client connects; host's `OnPlayerJoined` fires |
| Password: wrong / missing | Client joins with `xyz`, then with no password | `OnSessionJoined(WrongPassword)` after `Success` and the client leaves the session; no password → `PasswordRequired` |
| Password: split-screen guest | Client with 2 seats joins a password-protected host with the right password | Both seats admitted |
| Roster across travel | Host travels with 2 clients connected; one client quits during the travel | No leave events for the returning client; the quitter gets `OnPlayerLeft(ConnectionLost)` about 30 s later |
| Kick | Host calls `KickPlayer` on a client | Client disconnected; host gets `OnPlayerLeft(Kicked)` |
| Invites (Steam) | Host `SendInvite` to a friend; friend accepts from the overlay | `OnInviteSent(true)`; friend's `OnGameInviteReceived` fires; accept joins |
| PIE multi-window | 2+ PIE windows | Each uses its own `Context_N` online subsystem |
| Party debug | `dll.Multiplayer.PartyDebug 1` on the host during a split-screen join | Seat/reservation table appears at login and logout |

Record runs in `docs/sessions/` and promote results to this file and to `PRD.md` statuses.
