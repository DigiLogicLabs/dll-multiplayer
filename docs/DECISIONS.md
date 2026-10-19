# DLL Multiplayer — Decision log

Append-only. Never delete an entry; supersede it with a newer one that says **Supersedes:**.
Format: `## YYYY-MM-DD — <decision in one line>` then Decision / Reason / Rejected / Refs.

---

## 2026-08-04 — Travel emits `?listen` for every non-dedicated server

**Decision:** `UDLLMultiplayerTravelLibrary::TravelToLevel` routes on net mode (an active session is an extra
reason to ServerTravel) and appends `?listen` whenever `NetMode != NM_DedicatedServer`, logging URL + net mode +
session flag.
**Reason:** absolute ServerTravel drops `?listen`; keying on `NM_ListenServer` failed when the branch was entered
through an active session while the net mode read otherwise.
**Refs:** MEMORY 2026-08-04, 2026-08-05.

## 2026-09-14 — Split-screen guests online are judged as one party at the door

**Decision:** `ADLLMultiplayerGameSession` admits a machine's first seat only if every seat fits and reserves the
rest; unregistering checks for a shared platform id; the host advertises `MP_HUMANS` / `MP_MAXHUMANS`. Party info
rides the login options written by `UDLLMultiplayerLocalPlayer`.
**Reason:** the engine kicks the whole machine if any split join fails; Steam gives every seat the owner's id.
**Rejected:** an RPC handshake after login (a window where the server knows a seat but not its party).

## 2026-10-15 — dll-* naming standard; Apache-2.0

**Decision:** Digi Logic Labs open-source plugins follow [DLL_PLUGIN_STANDARD.md](DLL_PLUGIN_STANDARD.md)
(repo `dll-<kebab>`, plugin `DLL<Pascal>`, modules `DLL<Name>Runtime`…, types `UDLL<Name>*` / `FDLL<Name>*` /
`EDLL<Name>*`, CVars `dll.<Name>.*`) and are licensed Apache-2.0.
**Reason (owner, 2026-10-15):** "the most open source approach, our company will support these and use these over time."

## 2026-10-19 — Adopters with existing session code forward to the plugin instead of moving classes

**Decision:** When a game already has session or lobby classes, they keep their class paths and `UFUNCTION`
signatures and become one-line forwards to the plugin. The plugin never depends on a game module.
**Reason:** Blueprint nodes and Blueprint parent classes resolve by class path; moving a configured class (for
example the GameInstance) orphans assets unless a redirect is exactly right, and a wrong redirect stops the
project booting.
**Rejected:** physically relocating game classes into the plugin.
**Refs:** [INTEGRATION_GUIDE.md §13](INTEGRATION_GUIDE.md#13-if-your-game-already-has-session-code-forward-dont-move).

## 2026-10-19 — Session core: one operation tracker, continuations, one settings builder

**Decision:** Each async session operation (create, find, join, destroy, start) binds its online-service
completion when it starts and unbinds it when it ends (`TrackOp` / `FinishOp`); at most one of each kind is in
flight. "Leave, then do X" is a continuation (`LeaveThen`), and a newer continuation replaces an older one, so an
accepted invite outranks a queued re-create. Every create entry point builds `FDLLMultiplayerSessionCreationSettings`
and goes through `BuildOnlineSettings`; LAN is a property of the settings, not a separate path. `FindSessions`
goes through the filtered search. Every request reports its result exactly once, including synchronous refusals.
**Reason:** separate per-operation flags and handles drifted (a LAN re-create came back online; some early-outs
never reported, leaving menus spinning). One path per concern makes "reports exactly once" checkable.
**Rejected:** a "re-create after destroy" boolean; separate LAN and online create paths.
**Refs:** ARCHITECTURE §3 "Session operations"; MEMORY 2026-09-18.

## 2026-10-19 — Wire-level names are protocol and stay as they are

**Decision:** Login URL options `MPParty` / `MPPartySize` / `MPSeat` / `MPKey` / `Password`, refusal tokens
`MPParty.*` and `WrongPassword`, advertised session keys `MP_HUMANS` / `MP_MAXHUMANS` / `HasPassword` /
`MatchSettings`, and the primary asset type `MLSessionConfiguration` are protocol strings, not type names.
**Reason:** changing them breaks joins between builds, server browsers and Asset Manager rules.

## 2026-10-19 — Session passwords are checked by the host at login

**Decision:** The host keeps the password locally and advertises only `HasPassword`. The joining client appends
`?Password=` to the connect URL; `ADLLMultiplayerGameSession::ApproveLogin` checks it with
`UDLLMultiplayerSubsystem::IsSessionPasswordAccepted` and refuses with `WrongPassword`, which the client turns into
`OnSessionJoined(WrongPassword)` and a leave. Only a machine's first login is checked, identified by the
engine-written `SplitscreenCount`, never the client-supplied seat option. Passwords may not contain spaces or
`? = # &` because they travel in the URL.
**Reason:** online services do not check passwords themselves; the host's login is the one place a client cannot
skip. A split-screen guest's URL is rebuilt by the engine without the password and can only exist on a
connection that already passed.
**Consequence:** an invite cannot carry the password, so an invited player is refused by a password-protected
host; a join reports `Success` when the online join completes and may then report `WrongPassword`.

## 2026-10-19 — The player roster is host-side and survives map changes

**Decision:** The subsystem builds `SessionPlayers` from the GameMode's PostLogin / Logout events on the host only
and fires `OnPlayerJoined` / `OnPlayerLeft`. Logouts during a map change are ignored; 30 seconds after the last
one (`RosterReconcileDelaySeconds`) players who did not come back are reported as `ConnectionLost`. `KickPlayer`
goes through the game session and reports the leave as `Kicked`.
**Reason:** non-seamless travel logs every remote player out before the old world tears down; reporting those as
leaves would clear the lobby on every level change.

## 2026-10-19 — The plugin carries no game vocabulary

**Decision:** Match settings hold generic rules only; anything game-specific goes in `CustomSettings` or the
game's own struct. `FDLLMultiplayerTeamConfig::SpawnTag` is unset by default. The plugin defines no gameplay tags,
game modes or mechanics.
**Reason:** a generic session layer that ships one game's rules forces every other adopter to ignore or strip them.
