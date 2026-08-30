# H10 — R01–R08 completed-scope Microsoft contract hardening

H10 re-audits every Microsoft source contract attached to Rust responsibilities that had already been treated as completed from R01 through R08. The goal is not to turn all remaining native product backlog into Rust by relabeling it: the goal is to make it impossible for a completed Rust responsibility to silently remain or regress to `Missing`.

## Result

Two stale classifications move:

- `terminalCore/InputTest.cpp::InvalidKeyEvent`: **Partial -> Exact**. Commit `0e80c6b5765f4756ab31ef8884bdd8e2ec168228` already fixed the Microsoft invalid-key vectors (`VK=0` and `VK=255`, scan code `123`) and added the direct regression `microsoft_terminal_core_invalid_key_event_is_silent`. The ledger had not caught up with the product.
- `terminalApp/FzfTests.cpp::German_CaseMisMatch_FoldResultsInMultipleCodePoints`: **Missing -> Partial**. Microsoft itself marks this method `Ignore=true` because the current C++ matcher does not implement full Unicode case folding when one code point (`ß`) expands into multiple code points (`ss`). Rust owns the FZF matcher, German `ß` matching and ordinary case-insensitive folding, but likewise does not implement that expansion. `Partial` records the transferred owner and the precise remaining semantic gap without pretending the ignored Microsoft expectation passes.

The resulting global source-method distribution is:

```text
Exact         203
Stronger       11
Partial       351
Platform-only  63
UI-managed     22
Missing       448
Total        1098
```

## Completed-scope invariant

H10 adds `tools/rust/microsoft-rust-deferred-missing.json`, an explicit allowlist of source families whose `Missing` methods are genuine functional backlog rather than completed Rust ownership.

`Test-MicrosoftGlobalTestInventory.ps1` now fails when:

1. any effective `Missing` Microsoft contract belongs to a source family that is not explicitly deferred;
2. a deferred source disappears;
3. a deferred source no longer contains any `Missing` contracts but remains on the allowlist;
4. a pinned per-source deferred count changes where H10 records one;
5. the total number of explicitly deferred `Missing` contracts differs from the selected global coverage snapshot.

This closes the governance hole left by histogram-only checking: a completed `Exact`/`Partial`/boundary contract cannot fall back to `Missing` and hide among unrelated migration backlog.

Per-suite `expectedCoverage` now also supports the same priority model already used for global snapshots. Historical stage overlays remain immutable evidence; H10 supplies priority `10` expectations for `terminalCore` and `terminalApp` after the two deliberate reclassifications.

## R01–R08 sweep

- **R01 parser:** `Missing=0`.
- **R02/R03 adapter/input:** `Missing=0`.
- **R04 foundation:** 26 `Missing`, all explicitly deferred to real unported owners: Unicode search, UUID-v5, environment helpers, generational state, standalone size arithmetic and throttled scheduling.
- **R05 TerminalCore:** 4 `Missing`, all explicitly deferred to the not-yet-migrated aggregate terminal sizing/scroll notification owner. `InvalidKeyEvent` is now Exact.
- **R06 Host:** 210 `Missing`, all explicitly deferred Host aggregate backlog: aliases, API routines, clipboard extraction, history, input queue, ScreenBuffer, ICU search, C++ iterator surface and title translation.
- **R06-B Interactivity:** no `Missing`; deterministic UIA state is Exact where transferred and the rest remains Platform-only.
- **R07 Renderer:** the audited source seam contains Exact/Stronger/Partial contracts only; native graphics/UIA engines remain explicit native ownership.
- **R08 managed/product:** SettingsModel (157), LocalTerminalApp native product logic (38), TerminalApp JsonUtils (13) remain explicitly deferred. UnitControl is Platform-only, Tab/XAML remains UI-managed, all 38 active FZF methods remain Exact, and the one upstream-ignored FZF full-fold method is now Partial.

Therefore every `Missing` contract left after H10 is machine-declared deferred backlog. None is allowed to represent a responsibility that the R01–R08 Rust migration claims as completed.

## Safety

- Microsoft C++ product changes: **0**
- Rust product behavior changes in H10: **0**
- Microsoft tests removed or weakened: **0**
- Platform/UI boundaries reclassified as portable merely for coverage: **0**
- Certification gates relaxed: **0**
- R09 work started: **0**

TAEF remains the Microsoft runtime certification oracle. H10 strengthens source-method ownership accounting around it; it does not replace runtime certification.
