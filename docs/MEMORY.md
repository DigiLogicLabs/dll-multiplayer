# DLL Multiplayer — Project memory

Short, dated, verified facts and traps that the code doesn't make obvious. One line each, newest at the
bottom. Mark unverified items **UNVERIFIED**. Promote anything that becomes a rule into
[DECISIONS.md](DECISIONS.md). Rules: [../.github/instructions/ai-memory.instructions.md](../.github/instructions/ai-memory.instructions.md).

## Facts and traps

- 2026-08-04 — Absolute `ServerTravel` DROPS `?listen`: `FURL` inherits options only for RELATIVE travel and
  `UEngine::LoadMap` re-listens only when `!GIsClient || URL.HasOption("Listen")` (GIsClient is true on a listen
  server). Clients then fall back to `GameDefaultMap` — looks like "client loaded the wrong level".
- 2026-08-04 — Routing travel on "in an online session?" strands clients of a sessionless listen server, which is
  exactly what split-screen and PIE are. Route on `GetNetMode()`.
- 2026-08-05 — `ENetMode` values: 0 Standalone, 1 **Dedicated**, 2 Listen, 3 Client. A logged `netmode 1` is a
  dedicated server, which correctly needs no `?listen`.
- 2026-08-05 — Non-seamless ServerTravel (`bUseSeamlessTravel=False`) closes every client connection and each
  client reconnects; PIE clients don't reconnect. A "client dropped on travel" report is often this, not `?listen`.
- 2026-09-14 — `IOnlineSubsystem::Get()` is shared across PIE windows (each has its own `Context_N` OSS). Use
  `Online::GetSubsystem(World)`, resolved lazily in PIE (`GetWorldOnlineSubsystem`).
- 2026-09-14 — PIE "Listen Server" auto-connect is NOT an advertised session; test browsers with two Standalone
  instances.
- 2026-09-14 — The engine kicks the WHOLE machine if one split-screen guest fails login (World.cpp
  `PreLoginCompleteSplit`); Steam gives every local seat the owner's id, so a plain unregister on a guest's logout
  removes the owner from the online session.
- 2026-09-14 — `[/Script/Engine/GameSession]` (slash) is never read; the server cap silently stays at the
  engine's 16. Use `[/Script/Engine.GameSession]`.
- 2026-09-18 — `BuildUniqueId` must be `GetBuildUniqueId()`: OnlineSubsystemNull's LAN search discards results
  with a different build id, so a constant hides every LAN session.
- 2026-09-18 — A create that has to leave an existing session first must carry its LAN flag through the leave,
  or a LAN host comes back ONLINE. Every create early-out must end in `BroadcastCreateFailed` exactly once.
- 2026-10-16 — The Null subsystem fires some session completions BEFORE the call returns false; `FinishOp` guards
  against reporting the same request twice.
- 2026-10-16 — The editor half of `BuildPlugin` passing does NOT prove the game half: missing includes hide behind
  the editor's shared PCH (`UDataAsset`, `UBlueprintFunctionLibrary`, `FUniqueNetIdRepl`, `UGameInstance`). Always
  wait for the full `BUILD SUCCESSFUL`.
- 2026-10-19 — The seat limit is read by string from `[/Script/DLLMultiplayerRuntime.LocalPlayers] MaxLocalPlayers`
  in `DefaultGame.ini` (`UDLLMultiplayerLocalPlayerLibrary::GetMaxLocalPlayers`); `RegisterLocalPlayer` uses the
  same limit.
- 2026-10-19 — Dynamic delegate types are declared at file scope (in `DLLMultiplayerSubsystem.h` and
  `DLLMultiplayerQuickMatchSubsystem.h`), so their signature functions live at package level
  (`/Script/DLLMultiplayerRuntime.<Name>__DelegateSignature`), not under a class.
- 2026-10-19 — `FDLLMultiplayerSessionSearchFilter::bIncludeLAN` selects the query type (LAN query when true,
  online lobby query when false); it never searches both. It defaults to true.
- 2026-10-19 — `MPParty` / `MPSeat` / `MP_HUMANS` and the other `MP*` wire names are protocol strings, not type
  names; see DECISIONS 2026-10-19.
- 2026-10-19 — Non-seamless ServerTravel logs remote players out while `World->NextURL` is set, seconds before the
  old world tears down; the roster ignores those logouts and reconciles 30 s later. **UNVERIFIED** at runtime.
- 2026-10-19 — A wrong-password refusal reaches the client as a network failure whose error string is
  `WrongPassword`, after the online join already reported `Success`; the subsystem matches the failure to its own
  game instance (PIE runs several) before reporting `OnSessionJoined(WrongPassword)`. **UNVERIFIED** at runtime.
- 2026-10-19 — The Null (LAN) subsystem sends no invites and fires no invite-accepted events: `SendInvite` reports
  failure on LAN and `OnGameInviteReceived` only fires on platforms such as Steam or EOS.
- 2026-10-19 — The subsystem's `DrawDebugInfo` is not called by the plugin (no Tick); call it from your own HUD
  while you need it.
- 2026-10-19 — `EDLLMultiplayerSessionType` values (CampaignCoop, Horde…) are serialized in data assets; never
  reorder them.
