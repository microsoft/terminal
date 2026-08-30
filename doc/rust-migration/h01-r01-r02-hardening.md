# H01 — R01/R02 test-parity hardening

H01 starts from `rust/r08-product-integration@78cb6a0facfb87943eaa3cc9be5bb220b5805bce` and hardens the already-merged Microsoft-to-Rust equivalence ledger without changing product code.

## Goals

1. Reconcile R02 with the newer functional-lane TerminalInput audit, including downgrades where Windows keyboard-layout behavior is part of the Microsoft observation.
2. Replace the coarse R01 `OutputEngineTest.cpp = Partial` assumption with method-level `Exact` evidence for every Microsoft source method that already has a direct Microsoft-vector Rust contract.
3. Remove the OutputEngine family fallback entirely once all 64 methods are individually accounted for.

## R02 correction

The later functional audit showed two historical classifications were too optimistic:

| Microsoft contract | Before | H01 | Reason |
|---|---:|---:|---|
| `inputTest.cpp::TerminalInputModifierKeyTests` | Partial | Platform-only | The complete Microsoft method builds Windows keyboard state and observes active-layout `ToUnicodeEx` translation. Rust covers the deterministic VT-key subset, but that does not make the whole source method portable. |
| `inputTest.cpp::DifferentModifiersTest` | Exact | Partial | Rust covers the deterministic Backspace/Delete/Tab and slash/question outputs, but Microsoft obtains part of the key identity through Windows keyboard-layout translation. |

The hardened adapter distribution is therefore:

```text
adapter=72; runtime=411
Exact=19
Partial=52
Platform-only=1
Missing=0
```

This is intentionally stricter than the earlier `Exact=20, Partial=52` ledger.

## R01 OutputEngine hardening

`OutputEngineTest.cpp` contains 64 source methods. Delivery 11 conservatively represented all 64 through a source-family `Partial` rule even though six dedicated Rust contract files already carried direct Microsoft vectors.

H01 audits the whole source family and promotes **all 64 methods to Exact individually**:

- escape/CSI/parser-state vectors in `output_engine_microsoft_state_contract.rs`;
- OSC/DCS/SOS/PM/APC state/string vectors in `output_engine_microsoft_string_contract.rs`;
- cursor, positioning, mode and erase matrices in `output_engine_microsoft_dispatch_contract.rs`;
- SGR, DSR/device reports, terminal parameters, line-feed/control/tab/VT52 contracts in `output_engine_microsoft_report_contract.rs`;
- string processing, color/resource queries and resets, window title, clipboard, hyperlink and C1-mode contracts in `output_engine_microsoft_remaining_contract.rs`;
- XParse color assignment vectors in `output_engine_microsoft_color_contract.rs`.

The dispatch hardening is not inferred from names: the Rust tests reproduce the Microsoft parameter matrices, including the 12 numeric boundary values, both extra-parameter states, all 13 movement commands, the cursor row/column Cartesian product, all nine private modes, erase default/explicit variants and source ordering. Report and OSC witnesses likewise reproduce the full observations of their corresponding Microsoft source methods.

Every OutputEngine method now has its own machine-readable entry and concrete Rust witness. The former `OutputEngineTest.cpp = Partial` source-family rule has been removed; CI therefore fails if any of the 64 source identities is not represented individually.

The hardened R01 distribution becomes:

```text
terminal=98; runtime=760
Exact=83
Stronger=11
Partial=1
Platform-only=3
Missing=0
```

The only R01 `Partial` left is `InputEngineTest.cpp::C0Test`, whose portable C0/Alt behavior is present in Rust but whose complete Microsoft method also observes `VkKeyScanW`-derived modifier equivalence. The three platform-only InputEngine contracts remain Windows-owned.

Compared with the pre-H01 ledger, R01 alone moves **64 OutputEngine contracts from Partial to Exact** without changing product implementation.

## Global effect

```text
Before H01                 After H01
Exact          64          Exact         127
Stronger       11          Stronger       11
Partial       447          Partial       383
Platform-only  62          Platform-only  63
UI-managed     22          UI-managed     22
Missing       492          Missing       492
Total        1098          Total        1098
```

R02 deliberately moves one former Exact to Partial and one Partial to Platform-only. Against that stricter baseline, the completed OutputEngine audit produces a net global gain of **63 Exact contracts** while making one whole Microsoft method correctly platform-owned.

## Safety

- Product Rust changed: **0**
- Product C++ changed: **0**
- FFI changed: **0**
- Managed/XAML changed: **0**
- Microsoft tests removed or weakened: **0**
- Certification baselines relaxed: **0**

H01 changes only machine-readable classifications and migration documentation. The normal Rust/global witness gates must prove that every promoted semantic contract still references real Rust evidence.

## Next hardening increment

H02 should attack the first half of `adapterTest.cpp` (R03-A), promoting only contracts whose downstream observation is now materially owned in Rust rather than merely preserved as a deferred `OutputAction`.
