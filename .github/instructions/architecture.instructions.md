---
applyTo: "**"
---
# Architecture rules (DLL Multiplayer)

- **The engine's online subsystems do the networking.** We sit on `IOnlineSession` and engine travel; never
  re-implement NAT, transport or platform services.
- **Never depend on a game module or another dll-* plugin.** Game glue (campaign, run state, game modes) stays
  in the adopter, which forwards to the plugin. Bridges between dll-* plugins live in the adopter.
- **Forward, don't move** when generic code arrives from a game (class paths and signatures are API).
- **Server-authoritative:** travel, admission and kicks run on the server/host. `TravelToLevel` refuses on a client.
- **Party = one machine's seats.** Anything that admits, counts or unregisters players reasons about the
  whole party (`UDLLMultiplayerPartyRules`), not single logins; Steam shares one id across a machine's seats.
- Pure rules go in `UDLLMultiplayerPartyRules`-style function libraries with no world access, so tests can pin them.
- Game-specific rules are extension points (`OnPreHostTravel`, `OnPartyPlayerLoggedIn`, `GetLoginPlayerKey`),
  not branches.
- Update `docs/ARCHITECTURE.md` when a flow or extension point changes; check `docs/DECISIONS.md` first.
