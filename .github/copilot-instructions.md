# Copilot instructions — DLL Multiplayer

Follow [AGENTS.md](../AGENTS.md) (canonical). Modular rules in `.github/instructions/` apply by file glob.
Key points: never depend on a game module; forward, don't move; server-authoritative travel routed on net mode
with `?listen` restated; no Tick; no game vocabulary in new code; visual debug for every system; every change
leaves code + docs + memory; never claim "tested" without an artifact; only our own code (nothing copied from third-party sources).
