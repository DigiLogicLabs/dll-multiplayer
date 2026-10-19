# Review a diff

Review the current diff (or the given range) against the `AGENTS.md` non-negotiables. Read-only.

Check, citing `file:line`:
1. Dependency creep (`Build.cs`, includes from a game or another dll-* plugin).
2. Authority: travel, admission, kicks or session mutation reachable from a client.
3. Travel: routing on session state instead of net mode; `?listen` not restated on non-dedicated ServerTravel;
   missing URL/net-mode log line.
4. Party rules: single-login reasoning where a machine's party matters; unregistering a shared platform id.
5. Online subsystem: `IOnlineSubsystem::Get()` instead of per-world; delegate handles not cleared on every path;
   early-outs that never broadcast failure.
6. Tick, per-frame logging, timers left running.
7. Public API / Blueprint breaks: removed or renamed `UFUNCTION`/`UPROPERTY`, reordered public enums,
   `static_cast` between enums, missing redirects + CHANGELOG migration note.
8. Game vocabulary in new code; missing tooltips/categories; missing visual debug for a new system.
9. Logic without tests; tests depending on adopter config.
10. Docs/memory not updated (CHANGELOG, PRD, DECISIONS, MEMORY, TESTING).
11. Licensing: SPDX header on every file; nothing copied from tutorials, books, marketplace products or other
    third-party sources.

Report findings ranked by severity, each with a concrete failure scenario. Say "none" when clean.
