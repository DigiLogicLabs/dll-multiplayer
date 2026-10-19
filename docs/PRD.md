# DLL Multiplayer — Product Requirements

> **Owner:** Digi Logic Labs · **License:** Apache-2.0 ·
> **Status:** 1.0.0 beta, first public release pending adopter verification · **Reference adopter:** our own co-op game
> (UE 5.7, 1-4 split-screen + online, Steam)

## 1. Problem

Every co-op Unreal project rebuilds the same session and travel code, and the failures are invisible on the
host: clients stranded in the old level, a whole machine kicked because one split-screen guest did not fit,
the host unregistered from its own Steam session when a guest leaves, a browser showing a 4-person game as
"1/8". Our own games hit every one of these ([MEMORY.md](MEMORY.md)). We want to fix them once and
reuse the result in every Digi Logic Labs game, and offer it to anyone else building co-op in Unreal.

## 2. Users

| User | Needs |
|---|---|
| Gameplay/UI programmer | Host/find/join/leave and travel in a few calls, with readable failure text |
| Blueprint/UMG designer | Library nodes and a browser widget base; no C++ for a standard lobby |
| Couch co-op player | Join an online game with friends on the same screen, as one party |
| Online player | Accurate browser counts, quick match that keeps the party together |

## 3. Requirements

Status: ✅ built + evidence · 🟡 built, UNTESTED at runtime · ⬜ not built.

| ID | Requirement | Status |
|---|---|---|
| S1 | Create online / LAN / private sessions; from a data asset or settings struct (`UDLLMultiplayerSubsystem`) | 🟡 (standalone build green; PIE/LAN/Steam UNTESTED) |
| S2 | Find with filters (match type, ping, slots); browser rows for UI (`GetSessionListForUI`) | 🟡 |
| S3 | Join by info, index or direct address; leave/destroy | 🟡 |
| S4 | Session state machine + delegates for every transition and error | 🟡 |
| S5 | "Host the level I'm in" (`HostSessionAndTravelToLevel`) with a pre-travel hook (`OnPreHostTravel`) | 🟡 |
| S6 | Correct per-PIE-instance online subsystem | 🟡 (2026-09-14 fix) |
| L1 | Couch seats: add/remove local players, slot info, controller→seat, config seat limit (`UDLLMultiplayerLocalPlayerLibrary`) | 🟡 |
| T1 | Level travel that never strands clients (net-mode routing, `?listen` restated; `UDLLMultiplayerTravelLibrary`) | 🟡 |
| P1 | Split-screen guests online: whole-party admission + reservations (`ADLLMultiplayerGameSession`) | 🟡 |
| P2 | Shared platform id never unregisters a remaining player | 🟡 |
| P3 | Advertised human headcount read back by search results | 🟡 |
| P4 | Party-aware quick match (`UDLLMultiplayerQuickMatchSubsystem`) | 🟡 |
| D1 | Visual debug: subsystem HUD toggles, `dll.Multiplayer.PartyDebug` | 🟡 |
| S7 | Password-protected sessions enforced by the host at login (`IsSessionPasswordAccepted`, `WrongPassword`) | 🟡 |
| S8 | Host-side roster: `OnPlayerJoined` / `OnPlayerLeft` with reasons, surviving map changes; `KickPlayer` | 🟡 (host only; clients use replicated PlayerStates) |
| S9 | Platform invites: `SendInvite`, `OnGameInviteReceived`, accepted invites joined automatically | 🟡 (needs an online service with invites; not Null/LAN) |
| G1 | No game dependency; standalone `BuildPlugin` passes | ✅ (1.0.0 source, 2026-10-19, UE 5.7 Win64) |
| G2 | Every file is original Digi Logic Labs work under Apache-2.0 | ✅ |
| G3 | Automation tests live in the plugin (`DLLMultiplayer.*`) | ⬜ |
| G4 | No game vocabulary: `DLL*` names throughout, no game mechanics or gameplay tags in the plugin | ✅ |

## 4. Non-goals

- Matchmaking services that merge separate groups into balanced matches (needs a backend).
- Replacing the engine online subsystems; we sit on `IOnlineSession`.
- Gameplay replication, game modes, scoring — the adopting game owns those. Match settings are stored and
  advertised; the game applies them.
- Voice chat (candidate optional module later).

## 5. Success metrics

- A new Digi Logic Labs project gets a working LAN + Steam lobby with split-screen guests without writing
  session code.
- Zero "client loaded the wrong level" reports caused by travel.
