# R08 OutputEngine XParse color equivalence

This note closes the remaining portable `OutputEngineTest` color-parsing evidence gap recorded earlier in R08. It supplements the central Microsoft-to-Rust matrix until that ledger is consolidated at the R08 exit checkpoint.

## Microsoft contracts

The following Microsoft `OutputEngineTest` methods are now covered by direct Rust product-level equivalents:

| Microsoft contract | Rust evidence | Classification |
|---|---|---|
| `TestOscSetDefaultForeground` | `output_engine_microsoft_color_contract::microsoft_output_osc_set_default_foreground_matches_all_reference_vectors` | Exact |
| `TestOscSetDefaultBackground` | `output_engine_microsoft_color_contract::microsoft_output_osc_set_default_background_matches_all_reference_vectors` | Exact |
| `TestOscSetColorTableEntry` | `output_engine_microsoft_color_contract::microsoft_output_osc_set_color_table_entry_matches_valid_partial_and_invalid_vectors` | Exact |

## Product behavior

`rust/terminal-parser/src/output_engine_compat.rs` normalizes Microsoft/XParse color forms before delegating OSC dispatch to the R01 protocol core. The compatibility layer implements the semantics that differ from generic CSS-style color parsing:

- `#rgb`, `#rrggbb`, `#rrrgggbbb`, and `#rrrrggggbbbb` use the high bits of each component, matching XParse rather than scaling the component to 8 bits.
- XOrg color names used by the Microsoft vectors are ASCII case-insensitive and ignore ASCII whitespace.
- OSC 4 normalizes only color fields while preserving palette indices.
- OSC 10 through 19 normalize each resource color field independently.
- Invalid or partial color fields remain invalid; normalization does not turn malformed input into a successful dispatch.

The Rust contract tests reproduce the Microsoft foreground, background, and color-table vectors, including `#111`, `#123456`, `DarkOrange`, `orange`, multiple resources/table entries, partial invalid fields, and completely invalid fields.

## Gate consequence

These three methods may leave the per-change **semantic** boundary set for Rust-only implementation changes because their portable behavior is now Exact. They are **not removed from certification**: the complete Microsoft Terminal Suite remains mandatory at R08 exit and again in R09, and any future C ABI or C++ consumer change affecting this boundary makes the relevant Microsoft tests blocking again.
