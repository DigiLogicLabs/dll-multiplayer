# AGENTS.md — DLL Multiplayer

Canonical instructions for AI coding agents (Codex, Claude Code, Copilot, others) and a fast orientation for
human contributors. `CLAUDE.md` and `.github/copilot-instructions.md` point here — **edit this file, not them.**

## What this repo is

A Digi Logic Labs Unreal Engine 5 plugin: the reusable co-op / online session layer (sessions, couch
split-screen seats, split-screen guests online, quick match, client-safe level travel). Goals and scope:
[docs/PRD.md](docs/PRD.md). Reference adopter: our own co-op game (see the README).

**Status: 1.0.0 beta, first public release pending adopter verification** (standalone build + adopter host/join
tests; [docs/TESTING.md](docs/TESTING.md)). Every file is our own work. Never copy code from tutorials, books,
marketplace products or other third-party sources into this repo.

## Read before non-trivial work

1. [docs/README.md](docs/README.md) — knowledge-graph index
2. [docs/PRD.md](docs/PRD.md) — requirements (IDs like `S1`, `P2`) and non-goals
3. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — classes, flows, extension points
4. [docs/DECISIONS.md](docs/DECISIONS.md) — the *why*; don't relitigate a recorded decision without new evidence
5. [docs/MEMORY.md](docs/MEMORY.md) — verified facts and traps (travel, Steam ids, PIE subsystems)

## Layout

```
DLLMultiplayer.uplugin                       plugin descriptor (plugin name DLLMultiplayer)
Source/DLLMultiplayerRuntime/                single runtime module (export macro DLLMULTIPLAYERRUNTIME_API)
  Public/ DLLMultiplayerRuntimeModule.h      module class FDLLMultiplayerRuntimeModule
          DLLMultiplayerSubsystem.h          session subsystem + its Blueprint delegate types
          Session/DLLMultiplayerSessionLibrary.h      Blueprint session one-liners
          Player/DLLMultiplayerLocalPlayerLibrary.h   couch seats (FDLLMultiplayerPlayerSlotInfo)
          Travel/DLLMultiplayerTravelLibrary.h        client-safe level travel
          Party/  DLLMultiplayerGameSession.h, DLLMultiplayerLocalPlayer.h,
                  DLLMultiplayerPartyRules.h, DLLMultiplayerQuickMatchSubsystem.h
          Config/ DLLMultiplayerSessionConfiguration.h, DLLMultiplayerMatchSettings.h   data assets + match rules
          Types/  DLLMultiplayerSessionTypes.h, DLLMultiplayerSessionUITypes.h
          Widgets/DLLMultiplayerSessionBrowserWidget.h  abstract server-browser widget
  Private/                                   mirrors Public/
Config/FilterPlugin.ini                      ships docs/license with packaged builds
docs/                                        knowledge graph
.github/instructions, .github/prompts        modular rules + task prompts (tool-agnostic)
.claude/commands, .claude/agents             Claude Code wrappers + read-only reviewer
```

## Non-negotiables

1. **Never depend on a game.** Allowed modules: Core, CoreUObject, Engine, OnlineSubsystem(+Utils, +Steam on
   Win64), UMG, Slate(Core), InputCore, GameplayTags, Json(Utilities). Anything else → a new optional module.
   Never depend on another dll-* plugin.
2. **Forward, don't move.** When a game's generic code moves in, the game class keeps its path and signature
   and forwards to the plugin. Blueprint nodes and parent classes resolve by class path.
3. **Server-authoritative.** Travel, admission, kicks and session state changes happen on the server/host.
   Clients request; they never travel themselves.
4. **Route travel on net mode, not session state**, and re-state `?listen` on every non-dedicated ServerTravel
   (see MEMORY). Never "simplify" `UDLLMultiplayerTravelLibrary::TravelToLevel`.
5. **No Tick.** Online-subsystem callbacks, delegates, timers only while needed.
6. **Visual debug:** `bEnableVisualDebug` + granular toggles on the subsystem; the `dll.Multiplayer.PartyDebug`
   CVar. New systems add a master toggle, granular toggles, a `dll.Multiplayer.*` CVar and a screen HUD.
   No per-frame logging.
7. **Non-destructive.** Public API (`UFUNCTION`, `UPROPERTY`, public headers, enum value order) is additive
   between minors; breaking changes need a CHANGELOG migration note and a DECISIONS entry; renames use
   `[CoreRedirects]`. Never `static_cast` between two result enums — map with an explicit switch.
8. **No game vocabulary.** The plugin defines no game modes, mechanics or gameplay tags of its own; game
   rules are data or extension points.
9. **Honest status.** Never claim "compiles" or "tested" without an artifact. `UNTESTED` is valid.
10. **Only our content.** Only code and assets we wrote and can license under Apache-2.0; no marketplace,
    licensed or third-party-derived material.

## Coding conventions

- Types: `UDLLMultiplayer*`, `ADLLMultiplayer*`, `FDLLMultiplayer*`, `EDLLMultiplayer*`. Files are named after
  their main type without the prefix letter (`DLLMultiplayerGameSession.h`).
- Blueprint categories: `DLL Multiplayer|<Area>`. CVars: `dll.Multiplayer.*`. Log categories:
  `LogDLLMultiplayer*`.
- Every new file starts with:
  ```
  // Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
  // SPDX-License-Identifier: Apache-2.0
  ```
- Every exposed `UPROPERTY` has a doc comment (it becomes the tooltip) and a `Category`.
- Forward-declare in headers; include in `.cpp`. Engine-source claims in comments name the file/function.
- More detail: [.github/instructions/cpp.instructions.md](.github/instructions/cpp.instructions.md).

## Build & test

```powershell
# Standalone build (proves no hidden dependency on any game)
& "<UE>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin "-Plugin=<repo>\DLLMultiplayer.uplugin" "-Package=<short temp>\pkg" -TargetPlatforms=Win64 -Rocket
# Plugin tests (none yet; roadmap G3). Until then the pure-rule tests run in the reference adopter (docs/TESTING.md §2)
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests DLLMultiplayer; Quit" -unattended -nullrhi
```

Use a short `-Package` path (Windows 260-char limit). Only one UnrealBuildTool per engine install. Wait for the
full `BUILD SUCCESSFUL`: the editor half passing does not prove the game half (MEMORY 2026-10-16). Full matrix
(LAN, Steam, split-screen guests, travel): [docs/TESTING.md](docs/TESTING.md).

## Every change leaves Code + Docs + Memory

- Code + tests.
- Docs: the relevant `docs/*.md`, `CHANGELOG.md` under *Unreleased*, PRD requirement status if it changed.
- Memory: a dated line in `docs/MEMORY.md` for non-obvious facts/traps; a `docs/DECISIONS.md` entry for
  choices (append & supersede, never delete).
- Rules: [.github/instructions/ai-memory.instructions.md](.github/instructions/ai-memory.instructions.md).

## Task → prompt map

| Task | Prompt file | Claude Code |
|---|---|---|
| Start a session | `.github/prompts/verify-handoff.prompt.md` | `/verify-handoff` |
| Plan a feature | `.github/prompts/plan-feature.prompt.md` | `/plan-feature` |
| Add an optional integration module | `.github/prompts/add-integration-module.prompt.md` | `/add-integration-module` |
| Review a diff | `.github/prompts/review-diff.prompt.md` | `/review-diff` (or the `dll-multiplayer-reviewer` agent) |
| Cut a release | `.github/prompts/release.prompt.md` | `/release` |
| End a session | `.github/prompts/session-handoff.prompt.md` | `/session-handoff` |

Codex has no repo-shareable slash commands: ask it to follow the prompt file directly.

## Git

- Stage by path; never `git add -A` (adopting projects often have other tools writing nearby).
- Conventional, descriptive commit messages; one concern per commit.
- Don't commit `Binaries/`, `Intermediate/`, packaged output or personal settings
  (`.claude/settings.local.json`).
