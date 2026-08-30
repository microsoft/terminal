# F05 Adapter Partial audit

This audit records the remaining Microsoft `adapter` `Partial` contracts after the F05 response/parser closeout. It is a delivery-routing artifact, not a coverage exemption: every remaining row stays in the global R08 functional-debt gate until it becomes Exact/Stronger or is independently evidence-classified as a permitted non-functional boundary.

The modified sandwich order is:

1. F05 Adapter / Dispatch / VT responses
2. SettingsModel
3. TIL / Types / Foundation
4. Host / Console aggregate
5. TextBuffer / attributes / colors
6. Renderer / policy
7. TerminalCore
8. TerminalApp

## Remaining Adapter Partial inventory

| Microsoft contract | H11 class | Later owner | Evidence/rationale |
| --- | --- | --- | --- |
| `inputTest.cpp::TerminalInputTests` | platform-boundary | native keyboard boundary | Remaining branches are `MapVirtualKeyW`/default-character translation and non-KEY `INPUT_RECORD` dispatch. |
| `inputTest.cpp::TerminalInputNullKeyTests` | platform-boundary | native keyboard boundary | Remaining NUL VKEY lookup is `VkKeyScanExW` under a Windows layout. |
| `inputTest.cpp::DifferentModifiersTest` | platform-boundary | native keyboard boundary | Remaining identity observations depend on Windows keyboard-layout translation. |
| `kittyKeyboardProtocol.cpp::KeyPressTests` | functional | preexisting input/R02 burn-down | Major semantic families are present, but the complete Microsoft data-source table is not yet reproduced row-for-row. |
| `kittyKeyboardProtocol.cpp::IgnoreDeadKey` | platform-boundary | native keyboard boundary | Portable no-output semantic exists; `ToUnicodeEx` adapter remains platform-owned. |
| `adapterTest.cpp::ColorTableReportTests` | functional | Renderer / policy | Requires renderer color-table projection plus response formatting. |
| `adapterTest.cpp::Osc4ColorPaletteReportTests` | functional | Renderer / policy | Query routing exists; live renderer color-table lookup/formatting remains. |
| `adapterTest.cpp::XtermColorResourceReportTests` | functional | Renderer / policy | Resource query semantics exist; renderer alias resolution/formatting remains. |
| `adapterTest.cpp::AllowBlinkingTest` | functional | TextBuffer / attributes / colors | Requires concrete cursor blinking mutation on the text-buffer/product state. |
| `adapterTest.cpp::LineFeedTest` | functional | Host / Console aggregate + TextBuffer | Typed actions exist; buffer movement and host LineFeed-mode coupling remain. |
| `adapterTest.cpp::SetConsoleTitleTest` | functional | Host / Console aggregate | Payload preservation exists; product/window-title side effect remains. |
| `adapterTest.cpp::SetColorTableValue` | functional | Renderer / policy | Action/index domain exists; live renderer palette mutation remains. |
| `adapterTest.cpp::SoftFontSizeDetection` | functional | TIL / Types / Foundation + Renderer | Requires DRCS/FontBuffer cell-size inference and bitmap sizing semantics. |
| `adapterTest.cpp::MenuCompletionsTests` | functional | TerminalApp | Payload is lossless; completion parsing and UI/menu dispatch are external product behavior. |
| `adapterTest.cpp::SendC1ControlTest` | functional | Renderer / policy | S7C1T/S8C1T and TerminalInput side effects are owned; remaining assertions cross color-report serialization paths. |

## F05 closeout results

The historical Xterm color Partials became Exact after F04 supplied the live Rust `TextAttribute` owner: `Xterm256ColorTest`, `XtermExtendedColorDefaultParameterTest`, and `XtermExtendedSubParameterColorTest`.

`AnsiModeTest` and `TogglingC1ParserMode` are Exact through a narrow parser-control seam that mutates the canonical Rust `StateMachine`: ANSI/VT52 mode, `AcceptC1`, ISO-2022/ISO-8859-1 code page 28591, and UTF-8 code page 65001 are no longer reporting-only state.

`MacroInvokes` is Exact through `MacroExecutingProduct`. `CSI Ps * z` is intercepted by an acyclic decorator around the product dispatch, an owned macro sequence is prepared from the canonical `MacroBuffer`, and it is immediately fed back through the same live Rust `StateMachine` before the next outer code unit. Microsoft-derived witnesses cover default and maximum IDs, out-of-range and undefined IDs, output ordering, and the strict recursive depth of sixteen.

## F05 delivery status

**F05 is delivery-complete.** Adapter now has 56 Exact, 15 Partial, and 1 Platform-only source contracts. None of the remaining functional Adapter Partials is F05 response/parser integration debt; every one has a named later owner in the sandwich or belongs to older R02 input burn-down. They remain visible and blocking in the global R08 ledger.

The next delivery is the sandwich experiment's first early-integration slice: **SettingsModel core**, beginning with deterministic JSON/value deserialization and serialization rather than UI/editor projection.
