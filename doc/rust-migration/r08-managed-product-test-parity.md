# R08 managed/product test parity

Delivery 10 reconciles the Microsoft SettingsModel, Control, TerminalApp, and LocalTerminalApp source contracts without treating every application test as UI and without creating Rust implementations solely to reduce ledger debt.

The migration remains C++ to Rust, not C# or XAML to Rust. Existing XAML responsibility stays on the UI side when the contract truly requires XAML. Native C++ product semantics remain `Missing` until a real Rust owner exists. C++/WinRT control boundaries remain `Platform-only` when the test contract directly depends on WinRT, COM apartments, projected interfaces, or Windows control input surfaces.

## Frozen surface

The four R08 managed/product suites contribute 296 source `TEST_METHOD` contracts to the global 1,098-method census:

| Suite | Source methods | Runtime baseline | Delivery 10 result |
|---|---:|---:|---|
| `unitSettingsModel` | 157 | 157 | Missing=157 |
| `unitControl` | 28 | 29 | Platform-only=28 |
| `terminalApp` | 52 | 51 | Missing=52 |
| `localTerminalApp` | 59 | 76 | Missing=38, UI-managed=21 |
| **Total** | **296** | **313** | **Missing=247, Platform-only=28, UI-managed=21** |

Source-method counts and runtime invocation counts are deliberately different. The source census records contract identities; TAEF remains the runtime certification oracle.

## SettingsModel: 157 Missing

The current Rust workspace has no settings-model crate or other owner for these product semantics. They must not be labeled UI-managed merely because settings are eventually consumed by UI.

All methods in these source families therefore remain `Missing`:

- `ApplicationStateTests.cpp`
- `ColorSchemeTests.cpp`
- `CommandTests.cpp`
- `DeserializationTests.cpp`
- `KeyBindingsTests.cpp`
- `MediaResourceTests.cpp`
- `NewTabMenuTests.cpp`
- `ProfileTests.cpp`
- `SerializationTests.cpp`
- `TerminalSettingsTests.cpp`
- `ThemeTests.cpp`

This is useful backlog, not a test-parity failure to hide. JSON layering, profile behavior, commands, key bindings, themes, serialization, deserialization, and related settings-model behavior remain native product responsibilities until deliberately migrated.

## TerminalApp unit suite: 52 Missing

The `terminalApp` suite contains only two source families in the frozen census:

- `FzfTests.cpp`
- `JsonUtilsTests.cpp`

These are C++ product algorithms and utilities. They are not XAML orchestration, so Delivery 10 leaves all 52 methods `Missing`. A future Rust owner must provide real behavioral evidence before these classifications can improve.

## UnitControl: 28 Platform-only

`ControlCoreTests.cpp` and `ControlInteractivityTests.cpp` directly construct C++/WinRT control objects through COM apartments and projected settings/connection interfaces. The interactivity suite additionally exercises Windows pointer, mouse, wheel, focus, and control-event surfaces.

All 28 source methods therefore remain `Platform-only` at this boundary.

This does not erase lower-level Rust evidence. Selection, terminal input, buffer, renderer, and terminal-core semantics are already tracked in their owning migration stages. Crediting those same Rust tests again as replacements for the C++/WinRT control boundary would double-count evidence and incorrectly imply that the Windows control surface had moved to Rust.

## LocalTerminalApp: 38 Missing, 21 UI-managed

Three source families remain native C++ product behavior and have no Rust owner:

- `CommandlineTest.cpp`
- `FilteredCommandTests.cpp`
- `SettingsTests.cpp`

Together they account for 38 `Missing` contracts.

`TabTests.cpp` contains 21 methods and is categorically different. The suite is configured as UAP, explicitly activates XAML content, creates XAML controls, dispatches work to the UI thread, constructs `TerminalPage`, and depends on XAML application/window activation. Those 21 contracts are correctly `UI-managed` rather than `Missing`.

`UI-managed` is not a waiver. Those contracts stay in their appropriate managed/UI validation path and in full Microsoft certification; they simply are not Rust migration debt.

## Machine-enforced reconciliation

`tools/rust/microsoft-rust-equivalence-r08-managed.json` records source-level ownership rules and freezes the expected coverage distribution for all four suites.

`tools/rust/Test-MicrosoftGlobalTestInventory.ps1` now treats these suites as reconciled stages:

- `unitSettingsModel`
- `unitControl`
- `terminalApp`
- `localTerminalApp`

Every source method in those suites must therefore be covered by an explicit method entry or source rule. Falling through to the ledger default is an error, even when that default would also be `Missing`.

The overlay additionally freezes the expected distribution. A new, removed, or reclassified Microsoft contract cannot silently change the totals.

## Delivery 10 result

Delivery 10 intentionally adds no Rust product tests because no genuine transferred product semantic was found in these four suites that lacked evidence in its actual owning stage.

The result is stricter than manufacturing replacements:

- 296 of 296 source contracts deliberately classified.
- 247 honest product-migration backlog contracts remain visible.
- 28 C++/WinRT control-boundary contracts remain platform-only.
- 21 genuine XAML contracts remain UI-managed.
- No lower-level Rust evidence is double-counted as a replacement for an application or WinRT boundary.
- No Microsoft TAEF contract is removed or weakened.
- Full Microsoft certification remains authoritative.

The next delivery can use this explicit ownership map during the global parity sweep to distinguish real migration gaps from deliberate platform/UI ownership.
