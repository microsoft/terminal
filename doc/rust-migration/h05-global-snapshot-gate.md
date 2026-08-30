# H05 — versioned global coverage snapshot

The global Microsoft source-method distribution evolves as hardening deliveries promote or reclassify audited contracts. Keeping the only global expectation inside the historical R01 overlay forced later deliveries to rewrite R01 metadata even though R01 itself had not changed.

H05 makes that history explicit.

- Historical overlays may retain `expectedGlobalCoverage` with the default priority `0`.
- A later overlay may set `expectedGlobalCoveragePriority` together with `expectedGlobalCoverage`.
- The gate selects the highest-priority global snapshot.
- Two snapshots at the same highest priority remain an error.
- Per-suite `expectedCoverage` remains unique across overlays and unchanged.
- Source fingerprints, deliberate reconciliation, stale-entry detection, witness validation, and all coverage-class checks remain unchanged.

H05 uses priority `5` and freezes the corrected distribution after CI rejected the attempted `InvalidKeyEvent` promotion:

```text
Exact         136
Stronger       11
Partial       374
Platform-only  68
UI-managed     22
Missing       487
Total        1098
```

This is bookkeeping hardening only: it does not infer coverage from the expected snapshot, and it does not permit a mismatch. The calculated source-method distribution must still exactly equal the selected snapshot or CI fails.
