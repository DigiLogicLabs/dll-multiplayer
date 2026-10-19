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
