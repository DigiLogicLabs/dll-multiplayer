---
applyTo: "Source/**/*.{h,cpp,cs}"
---
# C++ rules (DLL Multiplayer)

Shared rules for every dll-* plugin: [docs/DLL_PLUGIN_STANDARD.md](../../docs/DLL_PLUGIN_STANDARD.md#coding-rules-shared-by-every-dll--plugin).
The rules below are specific to this plugin.

- **Dependencies:** the runtime `Build.cs` is a contract (engine online/UMG/Slate/GameplayTags/Json only).
  A new dependency goes in a new optional module (`.github/prompts/add-integration-module.prompt.md`).
- **Naming:** types `UDLLMultiplayer*` / `ADLLMultiplayer*` / `FDLLMultiplayer*` / `EDLLMultiplayer*`; files named
  after their main type without the prefix letter; Blueprint categories `DLL Multiplayer|<Area>`; CVars
  `dll.Multiplayer.*`; log categories `LogDLLMultiplayer*`.
- **Export macro:** `DLLMULTIPLAYERRUNTIME_API` on every public type.
- **Wire names are protocol:** login options (`MPParty`…), refusal tokens (`MPParty.*`), advertised keys
  (`MP_HUMANS`…) and the primary asset type stay as they are; renaming them breaks mixed-version joins.
- **Online subsystem:** resolve per world (`Online::GetSubsystem(World)`), never `IOnlineSubsystem::Get()` -
  PIE windows each have their own.
- **Delegates:** every `Add*Delegate_Handle` has a matching `Clear*` on completion AND on the synchronous
  failure path; every early-out broadcasts failure (no spinning menus).
- **Enums:** never reorder or insert mid-enum in public enums; map between enums with an explicit `switch`.
- **UPROPERTY:** doc comment above (tooltip), `Category`, clamps where numeric, `EditCondition` for debug
  sub-toggles.
- **No Tick, no per-frame logging.** Log full URL + net mode on every ServerTravel (diagnosability rule).
- **Engine-source claims** in comments cite the file/function they were verified against, with the UE version.
- **Public API is additive** between minors; renames use `[CoreRedirects]` + a CHANGELOG migration note.
