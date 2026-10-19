# DLL Multiplayer

**Co-op and online session layer for Unreal Engine 5.** By [Digi Logic Labs](https://digilogiclabs.com) ([GitHub](https://github.com/DigiLogicLabs)).
Built for, and developed alongside, our own UE games (reference adopter: MetalLegs — 1-4 player couch
split-screen plus online).

> **Status: 1.0.0 beta, first public release pending adopter verification.** The standalone `BuildPlugin` passes
> on UE 5.7 (2026-10-19); host/join tests in the reference adopter are **UNTESTED**
> ([docs/TESTING.md](docs/TESTING.md)).

## Why

Every co-op game rewrites the same session code, and the hard parts only fail for the people who **joined**:

- the host looks fine while clients are stranded in the old level (travel ran before the session existed, so the
  level never gained `?listen`);
- a split-screen guest connects alone and the whole couch is refused, or half of it gets in;
- a guest leaves and, because Steam gives every seat on a machine the same id, the **host's** registration is
  removed with it;
- quick match joins a game with one free slot while you have three people on the couch.

DLL Multiplayer is that layer, written once, with the engine-source reasons for each rule in the headers. Your
game says **what** it wants (host this level, find a game for my party, go to the next map); the plugin handles
the online subsystem, admission and travel.

## What's in 1.0.0

| Piece | What it does |
|---|---|
| `UDLLMultiplayerSubsystem` | GameInstance subsystem. Create (online / LAN / private / from a data asset), filtered find, join, leave, start; session state machine; password-protected sessions checked by the host; host-side roster (`OnPlayerJoined` / `OnPlayerLeft`, `KickPlayer`); invites through the online service (`SendInvite`, `OnGameInviteReceived`); local-player registry; ready state; `HostSessionAndTravelToLevel` ("open the level I'm in to friends") with an `OnPreHostTravel` hook. Blueprint delegates for each step (`OnSessionCreated`, `OnSessionsFound`, `OnSessionJoined`, `OnSessionError`, …). |
| `UDLLMultiplayerSessionLibrary` | Blueprint one-liners: `QuickHostSession` (optionally travelling to a lobby map) / `QuickHostLANSession` / `QuickHostPrivateSession`, `QuickFindSessions`, `QuickJoinSession`, `DirectJoinByAddress`, `LeaveCurrentSession`, browser rows (`GetSessionListForUI`), `CanHostSession`, ping text and colour, readable result messages. |
| `UDLLMultiplayerLocalPlayerLibrary` | Couch seats: count, max (from config), per-seat slot info, `AddNextLocalPlayer` ("press start to join"), add/remove a local player, controller → seat. |
| `UDLLMultiplayerTravelLibrary::TravelToLevel` | Party-wide level travel that never strands clients: picks `ServerTravel` vs `OpenLevel` from the net mode and session, and re-states `?listen` on absolute travel. Refuses on a client. |
| `ADLLMultiplayerGameSession` + `UDLLMultiplayerLocalPlayer` + `UDLLMultiplayerPartyRules` | Split-screen guests playing **online**: whole-party admission with seat reservations, shared-id-safe unregister, advertised human headcount, per-seat guest names. Debug: `dll.Multiplayer.PartyDebug 1`. |
| `UDLLMultiplayerQuickMatchSubsystem` | Party-aware quick match: rank games that fit the whole party (fuller first, then ping), fall through on a failed join, host one if none fit. |
| `UDLLMultiplayerSessionConfiguration`, `FDLLMultiplayerMatchSettings`, `UDLLMultiplayerMatchSettingsPreset` | Data-driven session settings (data asset per lobby type) and lobby match rules (limits, teams, mutators, custom key/values). |
| `UDLLMultiplayerSessionBrowserWidget` | Abstract UMG base for a server browser: filter, sort, select, join, Blueprint events. |

Works with any online subsystem the engine supports: `NULL` (LAN), Steam, EOS and others. It ships enabling
`OnlineSubsystem`, `OnlineSubsystemUtils` and `OnlineSubsystemSteam`.

## Install

1. Clone into your project's `Plugins/` folder:
   `git clone https://github.com/DigiLogicLabs/dll-multiplayer.git Plugins/dll-multiplayer`
2. Enable **DLL Multiplayer** in the Plugins window, or add it to your `.uproject`:
   `{ "Name": "DLLMultiplayer", "Enabled": true }`.
3. C++ users: add `"DLLMultiplayerRuntime"` to your module's `Build.cs` dependencies.
4. Pick an online subsystem in `DefaultEngine.ini` — see the [integration guide](docs/INTEGRATION_GUIDE.md#1-install-and-pick-an-online-subsystem).
   `NULL` gives LAN with no extra setup.

Targets **Unreal Engine 5.7** (Win64).

## Quick start

**Host the level you are in, and let friends join.**

```cpp
#include "DLLMultiplayerSubsystem.h"

if (UDLLMultiplayerSubsystem* Sessions = GetGameInstance()->GetSubsystem<UDLLMultiplayerSubsystem>())
{
    // Creation is async: the travel happens only after the session exists, so the level gains ?listen.
    Sessions->HostSessionAndTravelToLevel(TEXT("/Game/Levels/Hub"), /*NumConnections*/ 4, TEXT("Coop"), /*bLAN*/ true);
}
```

Blueprint: *Get Game Instance Subsystem* (class `DLLMultiplayerSubsystem`) → *Host Session And Travel To Level*.

**Browse and join.**

```cpp
#include "Session/DLLMultiplayerSessionLibrary.h"

UDLLMultiplayerSessionLibrary::QuickFindSessions(this, /*MaxResults*/ 50, /*bSearchLAN*/ true);
// Bind UDLLMultiplayerSubsystem::OnSessionsFound, then fill your list from:
TArray<FDLLMultiplayerSessionEntry> Rows = UDLLMultiplayerSessionLibrary::GetSessionListForUI(this);
// When the player picks a row:
UDLLMultiplayerSessionLibrary::QuickJoinSession(this, Rows[N].SessionIndex);
```

Blueprint: *Quick Find Sessions* → bind *On Sessions Found* on the subsystem → *Get Session List For UI* →
*Quick Join Session* with the row's `Session Index`.

**Move the whole party to the next level** (server only; clients follow).

```cpp
#include "Travel/DLLMultiplayerTravelLibrary.h"

UDLLMultiplayerTravelLibrary::TravelToLevel(this, TEXT("/Game/Levels/Arena"), TEXT(""),
    UDLLMultiplayerSessionLibrary::IsInSession(this));
```

Blueprint: *Travel To Level* (pass *Is In Session* into `Has Active Session`).

**Quick match for everyone on the couch.**

```cpp
#include "Party/DLLMultiplayerQuickMatchSubsystem.h"

FDLLMultiplayerQuickMatchRequest Request;
Request.MatchType = TEXT("Coop");
Request.HostLevelPath = TEXT("/Game/Levels/Hub"); // hosted if no game fits the party
GetGameInstance()->GetSubsystem<UDLLMultiplayerQuickMatchSubsystem>()->StartQuickMatch(Request);
```

Blueprint: *Get Game Instance Subsystem* (class `DLLMultiplayerQuickMatchSubsystem`) → *Start Quick Match* with
a *Make DLLMultiplayerQuickMatchRequest*; bind *On Quick Match State Changed* for progress text.

**Split-screen guests online:** set `GameSessionClass` to `DLLMultiplayerGameSession` on your GameMode and
`LocalPlayerClassName=/Script/DLLMultiplayerRuntime.DLLMultiplayerLocalPlayer` under `[/Script/Engine.Engine]`
in `DefaultEngine.ini`. The seat limit (default 4) is `MaxLocalPlayers` under
`[/Script/DLLMultiplayerRuntime.LocalPlayers]` in `DefaultGame.ini`.

## Design rules

- **Never depends on a game module.** Game glue stays in the game; games forward to the plugin.
- **Server-authoritative travel.** Clients never travel themselves; `TravelToLevel` refuses on a client.
- **Event-driven.** Online-subsystem callbacks and Blueprint delegates; nothing ticks.
- **Whole party or nobody.** A machine's seats are admitted, reserved and counted together.
- **Non-destructive.** Public signatures are stable between minor versions; renames ship with `[CoreRedirects]`.
- **Host decides.** Passwords and party admission are checked by the host at login, kicks run on the host; a
  client is never trusted to enforce them.

## Documentation

[docs/README.md](docs/README.md) is the index: [PRD](docs/PRD.md) (goals and requirements),
[architecture](docs/ARCHITECTURE.md), [integration guide](docs/INTEGRATION_GUIDE.md) (setup and every flow),
[testing](docs/TESTING.md), [roadmap](docs/ROADMAP.md), [decisions](docs/DECISIONS.md),
[memory/traps](docs/MEMORY.md).

**Working with AI assistants:** [`AGENTS.md`](AGENTS.md) is the canonical rule set (Claude Code, Codex and
Copilot all read it); the session workflow is in [docs/AI_WORKFLOW.md](docs/AI_WORKFLOW.md).

## Tests

The plugin does not ship automation tests yet. The party-rule and local-player tests run in the reference adopter;
roadmap item G3 moves the plugin-pure ones here under the `DLLMultiplayer.*` prefix, run with
`Automation RunTests DLLMultiplayer`. The manual LAN / Steam / split-screen / travel matrix and the recorded
results are in [docs/TESTING.md](docs/TESTING.md).

## Sister plugins

Part of the Digi Logic Labs `dll-*` plugin family (`dll-audio`, `dll-mass-units`, …) — see
[docs/DLL_PLUGIN_STANDARD.md](docs/DLL_PLUGIN_STANDARD.md).

## License

Free and open source under the [Apache License 2.0](LICENSE) (see also [NOTICE](NOTICE)). In plain words: use it
in any project, personal or commercial, change it, ship it in your games and redistribute it, with no fees or
royalties. When you redistribute the plugin or a modified copy, keep the `LICENSE` and `NOTICE` text with it (for a
shipped game, its credits or documentation is enough) and mark the files you changed. The license includes a patent
grant for this code; it does not grant use of the Digi Logic Labs name or logos, and the software comes without
warranty. Every file is original Digi Logic Labs work. Unreal Engine and its online subsystems are Epic Games products used under the Unreal Engine EULA and are not part of this repository. Contributions welcome - see [CONTRIBUTING.md](CONTRIBUTING.md).

More from Digi Logic Labs: [digilogiclabs.com](https://digilogiclabs.com).
