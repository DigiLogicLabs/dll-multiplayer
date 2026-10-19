---
applyTo: "Source/**/Tests/**"
---
# Testing rules (DLL Multiplayer)

- New plugin tests: `DLLMultiplayer.<Area>.<Behaviour>`; flags `EditorContext | EngineFilter`.
  (The existing pure-rule tests still live in the reference adopter; see `docs/TESTING.md` §2.)
- Tests must run in **any** project: no adopter config, maps or assets; transient objects only.
- Prefer pure rules (`UDLLMultiplayerPartyRules`) over world tests; online-subsystem behaviour belongs in the manual
  matrix in `docs/TESTING.md`, not automation.
- A test that passes before the fix proves nothing - when fixing a bug, check the test fails without it.
- Report results with the exact command and the pass/fail counts.
