# Add an optional integration module

Use when a feature needs a dependency the runtime module may not take (voice, EOS-specific APIs, CommonUI,
a third-party SDK).

1. Name: `DLLMultiplayer<Integration>` (e.g. `DLLMultiplayerVoice`). Folder `Source/DLLMultiplayer<Integration>/`.
2. `Build.cs`: depend on the runtime module + the integration's modules. Nothing in the runtime changes.
3. `.uplugin`: add the module (type, loading phase) and the engine plugin it needs under `"Plugins"`.
   Document in README what adopters must enable.
4. Register only through extension points (delegates, subclassable classes, subsystem API). If one is missing,
   add it to the runtime first as a generic, dependency-free hook.
5. Debug + tests + docs: `dll.Multiplayer.<Integration>.*` CVars, `DLLMultiplayer.<Integration>.*` tests,
   ARCHITECTURE module table, PRD status, DECISIONS entry, CHANGELOG.
6. Verify with the standalone `RunUAT BuildPlugin`.
