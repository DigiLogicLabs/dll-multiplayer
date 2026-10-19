# Plan a feature

Input: a feature request.

1. Map it to `docs/PRD.md` requirement IDs (add a new ID if none fits; say so).
2. Read `docs/ARCHITECTURE.md`, `docs/DECISIONS.md` and `docs/MEMORY.md`. Name any decision or trap it touches.
3. Decide the home: the runtime module (no new deps) or a new optional module. If it is game-specific, it
   belongs in the adopter behind an extension point. Justify.
4. Design: public types + signatures, data assets/settings, server vs client responsibilities, behaviour in
   standalone / listen / dedicated / split-screen / PIE multi-window / LAN / Steam, travel impact, visual debug,
   per-frame cost when idle (must be zero).
5. Tests: which pure-rule automation tests and which manual matrix rows in `docs/TESTING.md`.
6. Docs + memory: which files change (CHANGELOG, PRD status, ARCHITECTURE, DECISIONS, MEMORY).
7. Risks, and what would make you choose differently.

Output a plan; don't write code until it is approved.
