# Contributing to DLL Multiplayer

Thanks for helping! Digi Logic Labs maintains this plugin and uses it in our own games, so changes are
reviewed against real production use.

- **Issues:** include engine version, platform, online subsystem (NULL/LAN, Steam, EOS), net mode
  (standalone / listen / dedicated), local seat count, and a minimal repro.
- **Pull requests:** one change per PR; follow [docs/DLL_PLUGIN_STANDARD.md](docs/DLL_PLUGIN_STANDARD.md);
  add or update automation tests; add a CHANGELOG entry under *Unreleased*.
- **Scope:** the plugin never depends on a game. New engine-plugin or third-party dependencies go in a separate
  optional module.
- **Code and content:** only work you wrote and can license under Apache-2.0. No marketplace assets and no code
  copied from tutorials, books or other third-party sources.
- By submitting a contribution you agree it is licensed under the Apache License 2.0 (section 5 of the LICENSE).

## Getting started (fork and pull request)

This plugin is built alongside our own games: `main` is what we ship with, and it should always build. Anyone can
use it, fork it and send changes back.

1. **Fork** [dll-multiplayer](https://github.com/DigiLogicLabs/dll-multiplayer) on GitHub and clone your fork.
2. Put it in an Unreal Engine 5.7 C++ project as `Plugins/dll-multiplayer/` (any project; a blank C++ project is enough),
   enable **DLL Multiplayer** in *Edit → Plugins*, and add `"DLLMultiplayerRuntime"` to your module's `Build.cs` if you call it from C++.
3. Create a branch for one change (`fix/...`, `feature/...`).
4. Build the plugin on its own, which proves it has no hidden dependency on your project:
   `RunUAT.bat BuildPlugin "-Plugin=<path>\DLLMultiplayer.uplugin" "-Package=<short temp path>" -TargetPlatforms=Win64`
   must end with `BUILD SUCCESSFUL`.
5. This plugin has no in-plugin automation tests yet: check your change in a project that uses it (see
   [docs/TESTING.md](docs/TESTING.md)) and describe what you ran in the pull request.
6. Update the docs the change touches and add a line under *Unreleased* in [CHANGELOG.md](CHANGELOG.md).
7. Open a pull request against `main` and fill in the template. Small, focused PRs get reviewed fastest.

Please read the [Code of Conduct](CODE_OF_CONDUCT.md). Security problems go through [SECURITY.md](SECURITY.md),
not public issues.
