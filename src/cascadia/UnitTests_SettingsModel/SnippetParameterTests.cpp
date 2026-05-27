// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// SnippetParameterTests
// ---------------------
// Tests for the 🐣 Crawl tier of snippet parameters on `sendInput`
// (spec: doc/specs/#1595 - Suggestions UI/Snippet Parameters.md).
//
// Contract under test (authored against Parker's settings-model design):
//   * Microsoft.Terminal.Settings.Model.Parameter
//       - Name        : String (required, matches /^[a-zA-Z_][a-zA-Z0-9_]*$/)
//       - Description : String (optional)
//   * SendInputArgs.Parameters : IVector<Parameter> (optional, empty default)
//   * SendInputArgs.Resolve(IMap<String, String> values) : String
//       - replaces every `${<name>}` token in `Input` whose <name> appears
//         in `values` with the corresponding value
//       - leaves `${...}` tokens whose name contains a dot (e.g.
//         `${profile.name}`) untouched — those are reserved for the
//         existing Command.cpp profile/scheme expansion
//       - a leading backslash on the dollar sign (`\${foo}`) suppresses
//         substitution; the backslash is consumed, `${foo}` is emitted
//       - undeclared tokens (name not present in `values`) pass through
//         verbatim
//       - malformed tokens (`${unclosed`, `$missing`, `${}`,
//         `${has spaces}`) pass through verbatim
//   * Settings validation:
//       - `${<name>}` token referenced in `input` whose <name> is not in
//         the `parameters` array → SettingsLoadWarnings::SnippetParameterUndeclared
//       - `parameters` entry whose `name` is not referenced in `input`
//         → SettingsLoadWarnings::SnippetParameterUnused (warning, not error)
//       - `sendInput` with no `parameters` array → no warning, no
//         substitution, `Input()` is returned verbatim (compat)
//
// ⚠ BUILD NOTE for Mike / Parker / Dallas:
//   This file is written ahead of Parker's settings-model changes. If it
//   fails to compile against the current tip, that's the signal that
//   Parker's contract hasn't landed yet. The likely missing symbols are:
//     - Microsoft::Terminal::Settings::Model::Parameter
//     - SendInputArgs::Parameters()
//     - SendInputArgs::Resolve(IMap<hstring, hstring>)
//     - SettingsLoadWarnings::SnippetParameterUndeclared
//     - SettingsLoadWarnings::SnippetParameterUnused
//   If Parker landed under different spellings, fix the names here — the
//   scenarios are the contract, the spellings are negotiable.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/ActionAndArgs.h"
#include "../TerminalSettingsModel/ActionArgs.h"
#include "JsonTestClass.h"
#include "TestUtils.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Windows::Foundation::Collections;

namespace SettingsModelUnitTests
{
    class SnippetParameterTests : public JsonTestClass
    {
        TEST_CLASS(SnippetParameterTests);

        // -----------------------------------------------------------------
        // Substitution helper (SendInputArgs::Resolve)
        // -----------------------------------------------------------------
        TEST_METHOD(SnippetParameter_ResolveSingleOccurrence);
        TEST_METHOD(SnippetParameter_ResolveRepeatedOccurrence);
        TEST_METHOD(SnippetParameter_ResolveMultipleParameters);
        TEST_METHOD(SnippetParameter_ResolveUndeclaredTokenPassesThrough);
        TEST_METHOD(SnippetParameter_ResolveExtraValuesAreIgnored);
        TEST_METHOD(SnippetParameter_ResolveBackslashEscape);
        TEST_METHOD(SnippetParameter_ResolveLeavesDottedTokensAlone);
        TEST_METHOD(SnippetParameter_ResolveEmptyValueAllowed);
        TEST_METHOD(SnippetParameter_ResolveAcrossLineBreaks);
        TEST_METHOD(SnippetParameter_ResolveMalformedTokensPassThrough);

        // -----------------------------------------------------------------
        // SendInputArgs JSON round-trip
        // -----------------------------------------------------------------
        TEST_METHOD(SnippetParameter_RoundtripWithParameters);
        TEST_METHOD(SnippetParameter_RoundtripWithoutParametersIsCompatible);
        TEST_METHOD(SnippetParameter_EqualsAndHashRespectParameters);

        // -----------------------------------------------------------------
        // Settings validation warnings
        // -----------------------------------------------------------------
        TEST_METHOD(SnippetParameter_UndeclaredTokenProducesWarning);
        TEST_METHOD(SnippetParameter_UnusedParameterProducesWarning);
        TEST_METHOD(SnippetParameter_NoParametersArrayIsCompatible);

        // -----------------------------------------------------------------
        // Deferred — Walk-tier / requires UI-test infrastructure
        // -----------------------------------------------------------------
        // Superseded by the Walk-tier test plan below — the state machine
        // being tested has changed shape (no more `_searchBox`-as-input;
        // preview/composition channel is the surface). Kept as a redirect
        // so an old run still produces a visible "see Walk_*" pointer.
        TEST_METHOD(SnippetParameter_ParameterFillingStateMachine_TODO);

        // -----------------------------------------------------------------
        // 🚶 Walk-tier — inline template fill at the cursor
        //
        // Tracks the Walk-tier addendum:
        //   doc/specs/snippet-parameters-inline-fill-addendum.md
        //
        // Brady's load-bearing approvals (treat as canonical):
        //   1. Inline-template fill UX — YES
        //   2. Composition channel (PreviewInput-with-spans) — YES
        //   3. Tab at last tabstop = COMMIT, not wrap
        //
        // Naming convention:
        //   *_WALK_TODO    — model/logic test stub; the test body documents
        //                    the EXPECTED input/output table so the test is
        //                    half-written when the helper lands. Tagged with
        //                    the proposed helper API signature.
        //   *_WALK_TODO_UI — requires the UI test harness (composition
        //                    rendering, keyboard-input simulation, focus
        //                    management, cursor positioning). Explicit defer
        //                    per task constraint (D).
        //
        // Proposed helper APIs (DO NOT EXIST YET — flag for Parker / Dallas):
        //
        //   A. winrt::hstring SendInputArgs::ResolveForPreview(
        //          IMap<hstring,hstring> values,
        //          uint32_t currentTabstopIndex);
        //      Like Resolve, but empty-valued *declared* parameters render
        //      as the parameter NAME (placeholder semantics) instead of the
        //      empty string. Used by the inline preview to keep the user
        //      oriented before they start typing. Lives next to Resolve in
        //      src/cascadia/TerminalSettingsModel/ActionArgs.{h,cpp}.
        //
        //   B. std::vector<std::pair<std::wstring, SnippetPreviewSpanKind>>
        //      BuildSnippetPreviewSpans(
        //          std::wstring_view input,
        //          IVectorView<Parameter> parameters,
        //          IMap<hstring,hstring> values,
        //          uint32_t currentTabstopIndex);
        //      Pure free function. Walks `input` and emits an ordered list
        //      of (text, kind) runs ready for handoff to the composition
        //      channel (Ripley's PreviewInputSpans proposal). SpanKind
        //      ∈ { Normal, Placeholder, Active }. Preferred home:
        //      src/cascadia/TerminalSettingsModel/ActionArgs.{h,cpp} so the
        //      SettingsModel.UnitTests project can reach it without dragging
        //      in TerminalApp. Alternate home (Dallas's call):
        //      src/cascadia/TerminalApp/SuggestionsControl.{h,cpp} or a new
        //      utility translation unit alongside it.
        //
        //   C. enum class SnippetPreviewSpanKind { Normal, Placeholder, Active };
        //      Three kinds match Brady's UX:
        //        - Normal      — prose between parameters, or a typed mirror
        //                        of a repeated `${name}` that is NOT the
        //                        active occurrence.
        //        - Placeholder — empty-value slot; renders the parameter
        //                        NAME (dim/italic in the eventual styling).
        //        - Active      — the typed value at the current tabstop;
        //                        renders with the active-tabstop styling
        //                        (e.g. SetReverseVideo per Ripley's memo).
        //      No `ActivePlaceholder` kind — when the active slot is empty,
        //      Placeholder wins (the parameter name is what the user sees).
        //
        // Spec ambiguities flagged for MikeBot / Brady (see decision drop):
        //   - Shift+Tab at first tabstop: spec doesn't say. Tests below
        //     assume CLAMP (no-op). Open question.
        //   - Mirror-Active scope on repeated `${name}`: RESOLVED
        //     (Brady 2026-05-27) — multi-cursor model; ALL occurrences
        //     of the active tabstop render Active.
        // -----------------------------------------------------------------

        // (A) Settings-model layer — preview-mode hook
        TEST_METHOD(Walk_ResolveForPreview_EmptyValueRendersParameterName_WALK_TODO);
        TEST_METHOD(Walk_ResolveForPreview_TypedValueRendersValue_WALK_TODO);
        TEST_METHOD(Walk_ResolveForPreview_RepeatedTokenMirrorsValue_WALK_TODO);
        TEST_METHOD(Walk_ResolveForPreview_UndeclaredTokenPassesThrough_WALK_TODO);

        // (B) Tabstop ordering & navigation — UI surface
        TEST_METHOD(Walk_Tabstop_InitialIsFirstDeclaredParameter_WALK_TODO_UI);
        TEST_METHOD(Walk_Tabstop_OrderIsDeclarationOrderNotInputOrder_WALK_TODO_UI);
        TEST_METHOD(Walk_Tabstop_TabAdvancesForward_WALK_TODO_UI);
        TEST_METHOD(Walk_Tabstop_ShiftTabRetreats_WALK_TODO_UI);
        TEST_METHOD(Walk_Tabstop_TabAtLastTabstopCommits_WALK_TODO_UI);
        TEST_METHOD(Walk_Tabstop_ShiftTabAtFirstClamps_WALK_TODO_UI);

        // (C) Span construction for the inline preview — model-layer logic
        TEST_METHOD(Walk_Spans_SingleParam_EmptyValue_ActiveIndex_WALK_TODO);
        TEST_METHOD(Walk_Spans_SingleParam_TypedValue_ActiveIndex_WALK_TODO);
        TEST_METHOD(Walk_Spans_SingleParam_TypedValue_PastLastIndex_WALK_TODO);
        TEST_METHOD(Walk_Spans_TwoParams_SecondActive_FirstFilled_WALK_TODO);
        TEST_METHOD(Walk_Spans_TwoParams_FirstActive_SecondEmpty_WALK_TODO);
        TEST_METHOD(Walk_Spans_RepeatedToken_AllOccurrencesActive_WALK_TODO);
        TEST_METHOD(Walk_Spans_EmptyParamAtNonActiveSlot_RendersPlaceholder_WALK_TODO);

        // (D) UI rendering & input — explicitly deferred per task constraint (D)
        TEST_METHOD(Walk_UI_CompositionChannel_RendersSpansToTerminal_WALK_TODO_UI);
        TEST_METHOD(Walk_UI_KeystrokeUpdatesActiveTabstopLive_WALK_TODO_UI);
        TEST_METHOD(Walk_UI_RepeatedTokenMirrorsLiveAcrossOccurrences_WALK_TODO_UI);
        TEST_METHOD(Walk_UI_CursorPositionsAtActiveTabstop_WALK_TODO_UI);
        TEST_METHOD(Walk_UI_EscCancelsFillAndClearsPreview_WALK_TODO_UI);
        TEST_METHOD(Walk_UI_EnterOnLastTabstopCommitsAndDispatches_WALK_TODO_UI);
        TEST_METHOD(Walk_UI_TooltipShowsActiveParameterNameAndDescription_WALK_TODO_UI);
        TEST_METHOD(Walk_UI_IMECompositionPreemptsSnippetPreview_WALK_TODO_UI);

    private:
        // Mirror of DeserializationTests::createSettings — we need our own
        // copy here because that one is private. Keeps the inbox tiny so
        // settings->Warnings() only reflects warnings caused by the snippet
        // under test.
        static winrt::com_ptr<implementation::CascadiaSettings> createSettings(const std::string_view& userJSON)
        {
            static constexpr std::string_view inboxJSON{ R"({
                "profiles": [
                    {
                        "name": "Inbox Profile",
                        "guid": "{6239a42c-aaaa-49a3-80bd-e8fdd045185c}"
                    }
                ]
            })" };
            return winrt::make_self<implementation::CascadiaSettings>(userJSON, inboxJSON);
        }

        // Build a SendInputArgs from a raw `input` literal, with no parameters.
        // Used by the Resolve() tests — Resolve is content-only, the
        // parameters declaration is what drives the validation tests.
        static winrt::com_ptr<implementation::SendInputArgs> makeSendInputArgs(std::wstring_view input)
        {
            return winrt::make_self<implementation::SendInputArgs>(winrt::hstring{ input });
        }

        // Construct an IMap<hstring, hstring> for Resolve()
        static IMap<winrt::hstring, winrt::hstring> makeValueMap(std::initializer_list<std::pair<std::wstring_view, std::wstring_view>> entries)
        {
            auto map = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
            for (const auto& [k, v] : entries)
            {
                map.Insert(winrt::hstring{ k }, winrt::hstring{ v });
            }
            return map;
        }

        // Parse a single `sendInput` action json blob into a SendInputArgs +
        // any parse warnings. Mirrors what KeyBindingsTests::TestArbitraryArgs
        // does, but pulls just the args out so we can poke at .Parameters().
        static std::tuple<winrt::com_ptr<implementation::SendInputArgs>, std::vector<SettingsLoadWarnings>>
        parseSendInputArgsFromActionJson(const std::string_view& json)
        {
            const auto parsed = VerifyParseSucceeded(json);
            std::vector<SettingsLoadWarnings> warnings;
            const auto actionAndArgs = implementation::ActionAndArgs::FromJson(parsed, warnings);
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SendInput, actionAndArgs->Action());

            const auto iargs = actionAndArgs->Args();
            VERIFY_IS_NOT_NULL(iargs);

            winrt::com_ptr<implementation::SendInputArgs> args;
            args.copy_from(winrt::get_self<implementation::SendInputArgs>(iargs));
            VERIFY_IS_NOT_NULL(args);
            return { args, std::move(warnings) };
        }
    };

    // =====================================================================
    // Substitution helper (SendInputArgs::Resolve)
    // =====================================================================

    void SnippetParameterTests::SnippetParameter_ResolveSingleOccurrence()
    {
        // Spec: A single ${name} token gets replaced with the mapped value.
        const auto args = makeSendInputArgs(L"ssh ${host}");
        const auto values = makeValueMap({ { L"host", L"dev-box" } });
        const auto resolved = args->Resolve(values);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"ssh dev-box" }, resolved);
    }

    void SnippetParameterTests::SnippetParameter_ResolveRepeatedOccurrence()
    {
        // Spec, user story B: same parameter name referenced N times,
        // user fills once, all N occurrences get the same value.
        const auto args = makeSendInputArgs(L"git checkout -b ${branch} && git push -u origin ${branch}");
        const auto values = makeValueMap({ { L"branch", L"feature/x" } });
        const auto resolved = args->Resolve(values);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"git checkout -b feature/x && git push -u origin feature/x" }, resolved);
    }

    void SnippetParameterTests::SnippetParameter_ResolveMultipleParameters()
    {
        // Spec, user story C: multiple distinct parameters in one snippet.
        const auto args = makeSendInputArgs(L"ssh ${user}@${host}");
        const auto values = makeValueMap({ { L"user", L"zadji" }, { L"host", L"dev-box" } });
        const auto resolved = args->Resolve(values);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"ssh zadji@dev-box" }, resolved);
    }

    void SnippetParameterTests::SnippetParameter_ResolveUndeclaredTokenPassesThrough()
    {
        // Spec ("anything else in ${<...>} is treated as a literal string and
        // rendered untouched"): a ${name} whose <name> isn't in the value map
        // is left in place — Resolve is best-effort, it does not error.
        // (Settings-time validation is what catches the typo case — see
        // SnippetParameter_UndeclaredTokenProducesWarning below.)
        const auto args = makeSendInputArgs(L"echo ${foo}");
        const auto values = makeValueMap({});
        const auto resolved = args->Resolve(values);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"echo ${foo}" }, resolved);
    }

    void SnippetParameterTests::SnippetParameter_ResolveExtraValuesAreIgnored()
    {
        // Spec: if the map contains keys not referenced in the input,
        // they're simply not used. Resolve does not throw, does not warn.
        const auto args = makeSendInputArgs(L"echo hi");
        const auto values = makeValueMap({ { L"unused", L"value" } });
        const auto resolved = args->Resolve(values);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"echo hi" }, resolved);
    }

    void SnippetParameterTests::SnippetParameter_ResolveBackslashEscape()
    {
        // Spec ("Escaping literal ${...}"):
        //   \${foo}  →  ${foo}   (backslash consumed, no substitution)
        // Source string in C++ is `echo \${foo}` — note the doubled
        // backslash because C++ string escape.
        const auto args = makeSendInputArgs(L"echo \\${foo}");
        const auto values = makeValueMap({ { L"foo", L"bar" } });
        const auto resolved = args->Resolve(values);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"echo ${foo}" }, resolved);
    }

    void SnippetParameterTests::SnippetParameter_ResolveLeavesDottedTokensAlone()
    {
        // ${profile.name}, ${profile.icon}, ${scheme.name} are reserved for
        // the existing Command.cpp expansion (see Command.cpp:_expandCommand).
        // The Resolve helper must NOT touch them — anything in ${...}
        // containing a `.` is out of scope per spec ("name must match
        // [a-zA-Z_][a-zA-Z0-9_]*").
        const auto args = makeSendInputArgs(L"newTab profile=${profile.name}");
        // Deliberately pass a "profile" key — the dotted form must be
        // preserved even if a partial match for the prefix exists.
        const auto values = makeValueMap({ { L"profile", L"ignored" } });
        const auto resolved = args->Resolve(values);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"newTab profile=${profile.name}" }, resolved);
    }

    void SnippetParameterTests::SnippetParameter_ResolveEmptyValueAllowed()
    {
        // Spec ("Empty values are allowed by default. If the user hits Tab
        // on an empty slot, that parameter resolves to the empty string.").
        const auto args = makeSendInputArgs(L"ssh ${host}");
        const auto values = makeValueMap({ { L"host", L"" } });
        const auto resolved = args->Resolve(values);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"ssh " }, resolved);
    }

    void SnippetParameterTests::SnippetParameter_ResolveAcrossLineBreaks()
    {
        // Spec ("we resolve all parameters once at dispatch time, then the
        // resolved multi-line input goes into the existing waitForSuccess
        // pipeline"). Substitution must work across \r\n line breaks.
        const auto args = makeSendInputArgs(L"line1 ${a}\r\nline2 ${b}");
        const auto values = makeValueMap({ { L"a", L"X" }, { L"b", L"Y" } });
        const auto resolved = args->Resolve(values);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"line1 X\r\nline2 Y" }, resolved);
    }

    void SnippetParameterTests::SnippetParameter_ResolveMalformedTokensPassThrough()
    {
        // Spec ("Anything else in ${<...>} is treated as a literal string
        // and rendered untouched"). Every malformed token below must
        // pass through verbatim — Resolve is not a parser, it's a
        // best-effort find-and-replace.
        struct testCase
        {
            std::wstring_view input;
            std::wstring_view expected;
            std::wstring_view note;
        };
        static constexpr std::array cases{
            testCase{ L"${unclosed", L"${unclosed", L"missing closing brace" },
            testCase{ L"$missing", L"$missing", L"no opening brace at all (just a $)" },
            testCase{ L"${}", L"${}", L"empty name" },
            testCase{ L"${has spaces}", L"${has spaces}", L"name contains a space (invalid identifier)" },
        };

        // Even with a populated value map, NONE of these should resolve —
        // their names don't match the spec's identifier regex.
        auto map = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
        map.Insert(L"unclosed", L"X");
        map.Insert(L"missing", L"X");
        // L"" key intentionally omitted — IMap forbids empty-string keys in
        // some Windows runtimes; the spec's `${}` case doesn't need a map
        // entry to prove pass-through.
        map.Insert(L"has spaces", L"X");

        for (const auto& tc : cases)
        {
            Log::Comment(NoThrowString().Format(L"Malformed-token case: %s — %s", tc.input.data(), tc.note.data()));
            const auto args = makeSendInputArgs(tc.input);
            const auto resolved = args->Resolve(map);
            VERIFY_ARE_EQUAL(winrt::hstring{ tc.expected }, resolved);
        }
    }

    // =====================================================================
    // SendInputArgs JSON round-trip
    // =====================================================================

    void SnippetParameterTests::SnippetParameter_RoundtripWithParameters()
    {
        // Parse a sendInput action with parameters, verify the model side,
        // then re-serialize and verify the parameters survived.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "ssh ${user}@${host}",
            "parameters":
            [
                { "name": "user", "description": "Your account on the dev box" },
                { "name": "host", "description": "Dev box hostname" }
            ]
        })" };

        auto [args, warnings] = parseSendInputArgsFromActionJson(json);

        // No warnings — every referenced token is declared, every declared
        // parameter is referenced.
        VERIFY_ARE_EQUAL(0u, warnings.size());

        const auto params = args->Parameters();
        VERIFY_IS_NOT_NULL(params);
        VERIFY_ARE_EQUAL(2u, params.Size());

        VERIFY_ARE_EQUAL(winrt::hstring{ L"user" }, params.GetAt(0).Name());
        VERIFY_ARE_EQUAL(winrt::hstring{ L"Your account on the dev box" }, params.GetAt(0).Description());
        VERIFY_ARE_EQUAL(winrt::hstring{ L"host" }, params.GetAt(1).Name());
        VERIFY_ARE_EQUAL(winrt::hstring{ L"Dev box hostname" }, params.GetAt(1).Description());

        // Round-trip the args back through JSON and re-parse. The second
        // parse must produce an Equal object to the first.
        const auto reserialized = implementation::SendInputArgs::ToJson(*args);
        const auto [reparsedI, reparsedWarnings] = implementation::SendInputArgs::FromJson(reserialized);
        VERIFY_IS_NOT_NULL(reparsedI);
        VERIFY_ARE_EQUAL(0u, reparsedWarnings.size());
        VERIFY_IS_TRUE(args->Equals(reparsedI));
    }

    void SnippetParameterTests::SnippetParameter_RoundtripWithoutParametersIsCompatible()
    {
        // Compatibility guarantee: a sendInput WITHOUT a parameters array
        // round-trips as if the feature didn't exist. .Parameters() is
        // empty (or null) and .Input() is verbatim.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "echo hello"
        })" };

        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        VERIFY_ARE_EQUAL(0u, warnings.size());

        const auto params = args->Parameters();
        // The exact shape (null vs. empty IVector) is Parker's call —
        // accept either, but Size() must be 0 if non-null.
        if (params)
        {
            VERIFY_ARE_EQUAL(0u, params.Size());
        }

        VERIFY_ARE_EQUAL(winrt::hstring{ L"echo hello" }, args->Input());

        // Round-trip: the resulting JSON should NOT add an empty parameters
        // array unless we explicitly want serialization symmetry. Either
        // shape (omitted or empty array) is acceptable — what matters is
        // that the re-parsed object Equals the original.
        const auto reserialized = implementation::SendInputArgs::ToJson(*args);
        const auto [reparsedI, reparsedWarnings] = implementation::SendInputArgs::FromJson(reserialized);
        VERIFY_IS_NOT_NULL(reparsedI);
        VERIFY_ARE_EQUAL(0u, reparsedWarnings.size());
        VERIFY_IS_TRUE(args->Equals(reparsedI));
    }

    void SnippetParameterTests::SnippetParameter_EqualsAndHashRespectParameters()
    {
        // Two SendInputArgs with the same input AND the same parameters
        // list are equal. Differing parameter sets break equality and
        // (in the common case) the hash.
        static constexpr std::string_view jsonA{ R"(
        {
            "action": "sendInput",
            "input": "ssh ${host}",
            "parameters": [ { "name": "host", "description": "h" } ]
        })" };
        static constexpr std::string_view jsonB{ R"(
        {
            "action": "sendInput",
            "input": "ssh ${host}",
            "parameters": [ { "name": "host", "description": "h" } ]
        })" };
        static constexpr std::string_view jsonC{ R"(
        {
            "action": "sendInput",
            "input": "ssh ${host}",
            "parameters": [ { "name": "host", "description": "DIFFERENT" } ]
        })" };
        static constexpr std::string_view jsonD{ R"(
        {
            "action": "sendInput",
            "input": "ssh ${host}"
        })" };

        auto [argsA, wa] = parseSendInputArgsFromActionJson(jsonA);
        auto [argsB, wb] = parseSendInputArgsFromActionJson(jsonB);
        auto [argsC, wc] = parseSendInputArgsFromActionJson(jsonC);
        // jsonD has an undeclared ${host} — we expect 1 warning (Undeclared)
        // but the args themselves still parse so we can compare.
        auto [argsD, wd] = parseSendInputArgsFromActionJson(jsonD);

        // A == B (same content)
        VERIFY_IS_TRUE(argsA->Equals(*argsB));
        VERIFY_ARE_EQUAL(argsA->Hash(), argsB->Hash());

        // A != C (different description)
        VERIFY_IS_FALSE(argsA->Equals(*argsC));

        // A != D (one has params, one doesn't, even though Input is the same)
        VERIFY_IS_FALSE(argsA->Equals(*argsD));
    }

    // =====================================================================
    // Settings validation warnings
    // =====================================================================

    void SnippetParameterTests::SnippetParameter_UndeclaredTokenProducesWarning()
    {
        // Spec ("If a snippet references ${<name>} in its input but never
        // declares <name> in its parameters array, that's a settings
        // error. We should treat it the way we treat other settings errors
        // in .wt.json parsing — log a warning…"). NB: spec language says
        // "error" but the implementation is a load warning, matching the
        // pattern used by InvalidColorSchemeInCmd et al.
        static constexpr std::string_view settingsJSON{ R"({
            "profiles": [
                {
                    "name": "p0",
                    "guid": "{6239a42c-bbbb-49a3-80bd-e8fdd045185c}"
                }
            ],
            "actions": [
                {
                    "name": "snippet with typo",
                    "id": "Test.UndeclaredParam",
                    "command":
                    {
                        "action": "sendInput",
                        "input": "echo ${foo}",
                        "parameters": [ { "name": "bar" } ]
                    }
                }
            ]
        })" };

        const auto settings = createSettings(settingsJSON);

        bool foundUndeclared = false;
        for (uint32_t i = 0; i < settings->Warnings().Size(); ++i)
        {
            if (settings->Warnings().GetAt(i) == SettingsLoadWarnings::SnippetParameterUndeclared)
            {
                foundUndeclared = true;
                break;
            }
        }
        VERIFY_IS_TRUE(foundUndeclared,
                       L"Expected SettingsLoadWarnings::SnippetParameterUndeclared because ${foo} is referenced in input but not declared in parameters.");
    }

    void SnippetParameterTests::SnippetParameter_UnusedParameterProducesWarning()
    {
        // Spec ("Symmetric case: if parameters declares a name that isn't
        // referenced in input, that's a warning (not an error) — we'll
        // show the snippet but just skip that slot during filling. The
        // most likely cause of this is a typo (`{$input.host}` instead of
        // `${input.host}`)…"). The snippet still loads.
        static constexpr std::string_view settingsJSON{ R"({
            "profiles": [
                {
                    "name": "p0",
                    "guid": "{6239a42c-cccc-49a3-80bd-e8fdd045185c}"
                }
            ],
            "actions": [
                {
                    "name": "snippet with unused param",
                    "id": "Test.UnusedParam",
                    "command":
                    {
                        "action": "sendInput",
                        "input": "echo hi",
                        "parameters": [ { "name": "unused" } ]
                    }
                }
            ]
        })" };

        const auto settings = createSettings(settingsJSON);

        bool foundUnused = false;
        for (uint32_t i = 0; i < settings->Warnings().Size(); ++i)
        {
            if (settings->Warnings().GetAt(i) == SettingsLoadWarnings::SnippetParameterUnused)
            {
                foundUnused = true;
                break;
            }
        }
        VERIFY_IS_TRUE(foundUnused,
                       L"Expected SettingsLoadWarnings::SnippetParameterUnused because the `unused` parameter is declared but not referenced in input.");

        // Spec: warning-only, not error. The snippet must still be loaded
        // and discoverable in the ActionMap.
        const auto action = settings->ActionMap().GetActionByID(L"Test.UnusedParam");
        VERIFY_IS_NOT_NULL(action, L"Snippet must still load even with the Unused warning.");
    }

    void SnippetParameterTests::SnippetParameter_NoParametersArrayIsCompatible()
    {
        // Compatibility test: a pre-feature sendInput with no parameters
        // array AND a ${foo}-shaped string in its input must:
        //   (1) load with no SnippetParameter* warnings
        //   (2) have Input() return the verbatim string
        //   (3) Not engage the substitution helper at all
        // This is the contract that protects every existing user's
        // settings.json from this feature.
        static constexpr std::string_view settingsJSON{ R"({
            "profiles": [
                {
                    "name": "p0",
                    "guid": "{6239a42c-dddd-49a3-80bd-e8fdd045185c}"
                }
            ],
            "actions": [
                {
                    "name": "legacy snippet with dollar-brace",
                    "id": "Test.NoParameters",
                    "command":
                    {
                        "action": "sendInput",
                        "input": "echo ${foo}"
                    }
                }
            ]
        })" };

        const auto settings = createSettings(settingsJSON);

        for (uint32_t i = 0; i < settings->Warnings().Size(); ++i)
        {
            const auto w = settings->Warnings().GetAt(i);
            VERIFY_ARE_NOT_EQUAL(SettingsLoadWarnings::SnippetParameterUndeclared, w,
                                 L"A snippet with no `parameters` array must NOT trigger Undeclared — substitution is opt-in.");
            VERIFY_ARE_NOT_EQUAL(SettingsLoadWarnings::SnippetParameterUnused, w,
                                 L"A snippet with no `parameters` array obviously can't have Unused warnings.");
        }

        const auto action = settings->ActionMap().GetActionByID(L"Test.NoParameters");
        VERIFY_IS_NOT_NULL(action);
        const auto args = action.ActionAndArgs().Args().try_as<SendInputArgs>();
        VERIFY_IS_NOT_NULL(args);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"echo ${foo}" }, args.Input());
    }

    // =====================================================================
    // Deferred — UI / state-machine tests live in a follow-up pass
    // =====================================================================

    void SnippetParameterTests::SnippetParameter_ParameterFillingStateMachine_TODO()
    {
        // SUPERSEDED. The Crawl-tier `_searchBox`-as-fill-input state
        // machine was retired by the Walk-tier pivot (MikeBot's addendum,
        // Brady-approved 2026-05-26). The new surface is the
        // PreviewInput / Composition channel; the new tests live in the
        // `Walk_*` section below. This stub stays as a breadcrumb so an
        // old run still produces a visible pointer to the new tests.
        //
        // See:
        //   doc/specs/snippet-parameters-inline-fill-addendum.md
        //   .squad/decisions.md (2026-05-26 Walk-tier entries)
        Log::Comment(L"SUPERSEDED — see Walk_* tests below.");
    }

    // =====================================================================
    // 🚶 Walk-tier — model-layer preview hook (A)
    // =====================================================================
    //
    // These tests assume a NEW API `SendInputArgs::ResolveForPreview` that
    // mirrors `Resolve` but renders empty-valued declared parameters as the
    // parameter NAME (placeholder) instead of empty string. This is the
    // semantic the inline preview needs so the user can SEE what they're
    // filling in before they start typing (fixes Brady's "the thing I'm
    // filling disappears" complaint).
    //
    // The dispatch-time Resolve semantics (empty → empty) are unchanged —
    // already covered by SnippetParameter_ResolveEmptyValueAllowed above.

    void SnippetParameterTests::Walk_ResolveForPreview_EmptyValueRendersParameterName_WALK_TODO()
    {
        // INPUT:
        //   input            = L"git checkout ${branch}"
        //   parameters       = [ { Name: L"branch" } ]
        //   values           = { branch -> L"" }
        //   currentTabstop   = 0
        //
        // EXPECTED:
        //   args->ResolveForPreview(values, 0) == L"git checkout branch"
        //
        // Rationale: declared-but-empty parameters render as their NAME in
        // preview mode (placeholder text). Contrast with dispatch-time
        // Resolve, which renders them as empty (see
        // SnippetParameter_ResolveEmptyValueAllowed).
        //
        // WALK_TODO_API: SendInputArgs::ResolveForPreview(values, index)
        //   in src/cascadia/TerminalSettingsModel/ActionArgs.{h,cpp}
        Log::Comment(L"WALK_TODO: requires SendInputArgs::ResolveForPreview helper.");
    }

    void SnippetParameterTests::Walk_ResolveForPreview_TypedValueRendersValue_WALK_TODO()
    {
        // INPUT:
        //   input            = L"git checkout ${branch}"
        //   parameters       = [ { Name: L"branch" } ]
        //   values           = { branch -> L"main" }
        //   currentTabstop   = 0
        //
        // EXPECTED:
        //   args->ResolveForPreview(values, 0) == L"git checkout main"
        //
        // Rationale: once the user has typed anything, preview mode behaves
        // exactly like Resolve — the placeholder semantics only apply to
        // the empty case.
        //
        // WALK_TODO_API: SendInputArgs::ResolveForPreview(values, index)
        Log::Comment(L"WALK_TODO: requires SendInputArgs::ResolveForPreview helper.");
    }

    void SnippetParameterTests::Walk_ResolveForPreview_RepeatedTokenMirrorsValue_WALK_TODO()
    {
        // INPUT:
        //   input            = L"git checkout -b ${branch} && git push -u origin ${branch}"
        //   parameters       = [ { Name: L"branch" } ]
        //   values           = { branch -> L"feature/x" }
        //   currentTabstop   = 0
        //
        // EXPECTED:
        //   args->ResolveForPreview(values, 0) ==
        //     L"git checkout -b feature/x && git push -u origin feature/x"
        //
        // Rationale: mirror typing — both occurrences of ${branch} render
        // the typed value. This is the same Resolve semantic already
        // verified at dispatch by SnippetParameter_ResolveRepeatedOccurrence;
        // this test asserts ResolveForPreview honors it too.
        //
        // WALK_TODO_API: SendInputArgs::ResolveForPreview(values, index)
        Log::Comment(L"WALK_TODO: requires SendInputArgs::ResolveForPreview helper.");
    }

    void SnippetParameterTests::Walk_ResolveForPreview_UndeclaredTokenPassesThrough_WALK_TODO()
    {
        // INPUT:
        //   input            = L"echo ${foo}"
        //   parameters       = [] (none declared)
        //   values           = {}
        //   currentTabstop   = 0
        //
        // EXPECTED:
        //   args->ResolveForPreview(values, 0) == L"echo ${foo}"
        //
        // Rationale: undeclared tokens are NOT tabstops; they render
        // literally in the preview. Matches dispatch-time Resolve behavior
        // (see SnippetParameter_ResolveUndeclaredTokenPassesThrough).
        // Spec §6.5: "Undeclared ${foo} in input is NOT a tabstop."
        //
        // WALK_TODO_API: SendInputArgs::ResolveForPreview(values, index)
        Log::Comment(L"WALK_TODO: requires SendInputArgs::ResolveForPreview helper.");
    }

    // =====================================================================
    // 🚶 Walk-tier — tabstop ordering & navigation (B)
    // =====================================================================
    //
    // Tabstop navigation state lives in SuggestionsControl (currently the
    // Crawl-tier `_paramFilling.currentIndex`). Walk's pivot may relocate
    // this state into the new inline-fill controller — wherever it lands,
    // the navigation semantics below must hold.
    //
    // These are UI-test stubs because tabstop transitions are observable
    // only through focus / cursor position / span styling — none of which
    // the SettingsModel.UnitTests project can reach. They are recorded
    // here as the canonical contract so Dallas (Phase B) and the eventual
    // UI-test harness owner have a single source of truth.

    void SnippetParameterTests::Walk_Tabstop_InitialIsFirstDeclaredParameter_WALK_TODO_UI()
    {
        // Spec §6.1: "Initial tabstop is the first entry in the snippet's
        // parameters array. NOT the first ${name} token encountered while
        // scanning input left to right."
        //
        // SCENARIO:
        //   parameters       = [ "branch", "remote" ]
        //   input            = L"git push ${remote} ${branch}"
        //   on engage:
        //     currentTabstopIndex == 0  (the `branch` slot, not `remote`)
        //     active span = ${branch}'s position in input
        //
        // WALK_TODO_UI: needs SuggestionsControl-fill-engage test harness.
        Log::Comment(L"WALK_TODO_UI: tabstop-engage scenario — verify initial index = 0 (first declared).");
    }

    void SnippetParameterTests::Walk_Tabstop_OrderIsDeclarationOrderNotInputOrder_WALK_TODO_UI()
    {
        // Spec §6.2: "Order is declaration order. If parameters is
        // [branch, remote] but input is `git push ${remote} ${branch}`,
        // the initial tabstop is still `branch`."
        //
        // SCENARIO:
        //   parameters       = [ "branch", "remote" ]
        //   input            = L"git push ${remote} ${branch}"
        //   Tab → currentTabstopIndex moves 0 → 1 (now on `remote`)
        //   The active-span POSITION jumps backward in input (left of where
        //   it just was), confirming declaration order drives navigation.
        //
        // WALK_TODO_UI: needs Tab-key simulation.
        Log::Comment(L"WALK_TODO_UI: verify Tab navigates by declaration order, not input order.");
    }

    void SnippetParameterTests::Walk_Tabstop_TabAdvancesForward_WALK_TODO_UI()
    {
        // SCENARIO:
        //   parameters       = [ "a", "b", "c" ]
        //   currentIndex     = 0 → Tab → 1 → Tab → 2
        //
        // WALK_TODO_UI: needs Tab-key simulation in the inline-fill controller.
        Log::Comment(L"WALK_TODO_UI: Tab advances currentTabstopIndex by 1.");
    }

    void SnippetParameterTests::Walk_Tabstop_ShiftTabRetreats_WALK_TODO_UI()
    {
        // SCENARIO:
        //   parameters       = [ "a", "b", "c" ]
        //   currentIndex     = 2 → Shift+Tab → 1 → Shift+Tab → 0
        //   Previously-typed values persist on revisit (Spec §6.9).
        //
        // WALK_TODO_UI: needs Shift+Tab simulation and value-persistence verification.
        Log::Comment(L"WALK_TODO_UI: Shift+Tab retreats and preserves typed values.");
    }

    void SnippetParameterTests::Walk_Tabstop_TabAtLastTabstopCommits_WALK_TODO_UI()
    {
        // BRADY-APPROVED (2026-05-26): Tab at last tabstop = COMMIT, not wrap.
        // Spec §7.3 take-two: "Tab-on-last commits (synonym for Enter)".
        //
        // SCENARIO:
        //   parameters       = [ "a", "b" ]
        //   currentIndex     = 1 (last)
        //   user types value into b, then presses Tab
        //   EXPECTED: dispatch fires (same path as Enter); inline preview
        //             clears; resolved string is sent to the shell.
        //   NOT EXPECTED: currentIndex wraps to 0.
        //
        // WALK_TODO_UI: needs dispatch-pipeline observation + Tab simulation.
        Log::Comment(L"WALK_TODO_UI: Tab at last tabstop commits (Brady — not wrap).");
    }

    void SnippetParameterTests::Walk_Tabstop_ShiftTabAtFirstClamps_WALK_TODO_UI()
    {
        // SPEC AMBIGUITY (open question for MikeBot / Brady):
        //   Spec §7.3 picks Tab-at-last = commit, but is silent on
        //   Shift+Tab at first. This test ASSUMES CLAMP (no-op) as the
        //   sane default — the alternative (wrap to last) would feel
        //   inconsistent with Tab-at-last's non-wrap semantic.
        //
        // SCENARIO:
        //   parameters       = [ "a", "b" ]
        //   currentIndex     = 0 (first)
        //   user presses Shift+Tab
        //   EXPECTED (chosen default): currentIndex stays 0 (no-op).
        //   ALTERNATIVE (if Brady picks): currentIndex wraps to 1.
        //
        // WALK_TODO_UI: needs Shift+Tab simulation. Re-baseline test once
        // Brady resolves the ambiguity.
        Log::Comment(L"WALK_TODO_UI: Shift+Tab at first tabstop clamps (default — Brady to confirm).");
    }

    // =====================================================================
    // 🚶 Walk-tier — span construction for inline preview (C)
    // =====================================================================
    //
    // These tests document the EXPECTED output of the proposed
    // `BuildSnippetPreviewSpans` helper (see Walk-tier header above). The
    // helper is a pure function from
    //   (input, parameters, values, currentTabstopIndex)
    // to an ordered list of (text, kind) runs. Three kinds:
    //   Normal      — prose, or a typed mirror that is NOT the active slot
    //   Placeholder — empty-value slot rendered as the parameter NAME
    //   Active      — the typed value at the current tabstop
    //
    // No `ActivePlaceholder` kind — when the active slot is empty,
    // Placeholder wins (the parameter name is what the user sees).
    //
    // Each test body documents the input table and expected span sequence;
    // when the helper lands, swap `Log::Comment` for the actual
    // VERIFY_ARE_EQUAL loop.

    void SnippetParameterTests::Walk_Spans_SingleParam_EmptyValue_ActiveIndex_WALK_TODO()
    {
        // INPUT:
        //   input            = L"git checkout ${branch}"
        //   parameters       = [ { Name: L"branch" } ]
        //   values           = { branch -> L"" }
        //   currentTabstop   = 0
        //
        // EXPECTED SPANS (in order):
        //   { L"git checkout ", Normal }
        //   { L"branch",        Placeholder }
        //
        // Rationale: active tabstop with empty value renders the parameter
        // NAME as placeholder text (Spec §6.6).
        //
        // WALK_TODO_API: BuildSnippetPreviewSpans(input, params, values, index)
        Log::Comment(L"WALK_TODO: requires BuildSnippetPreviewSpans helper.");
    }

    void SnippetParameterTests::Walk_Spans_SingleParam_TypedValue_ActiveIndex_WALK_TODO()
    {
        // INPUT:
        //   input            = L"git checkout ${branch}"
        //   parameters       = [ { Name: L"branch" } ]
        //   values           = { branch -> L"main" }
        //   currentTabstop   = 0
        //
        // EXPECTED SPANS (in order):
        //   { L"git checkout ", Normal }
        //   { L"main",          Active }
        //
        // Rationale: typed value at the current tabstop gets Active
        // styling — the spec's "tabstop active" treatment (Spec §6.6).
        //
        // WALK_TODO_API: BuildSnippetPreviewSpans(input, params, values, index)
        Log::Comment(L"WALK_TODO: requires BuildSnippetPreviewSpans helper.");
    }

    void SnippetParameterTests::Walk_Spans_SingleParam_TypedValue_PastLastIndex_WALK_TODO()
    {
        // INPUT:
        //   input            = L"git checkout ${branch}"
        //   parameters       = [ { Name: L"branch" } ]
        //   values           = { branch -> L"main" }
        //   currentTabstop   = 1 (past-last — i.e. committed / pre-dispatch)
        //
        // EXPECTED SPANS (in order):
        //   { L"git checkout main", Normal }
        //
        // Rationale: no active tabstop → no Active span. The entire
        // resolved string renders flat as Normal. Adjacent Normal runs
        // SHOULD be coalesced for renderer efficiency (the
        // ${branch}-substitution and the surrounding prose merge into one).
        //
        // WALK_TODO_API: BuildSnippetPreviewSpans(input, params, values, index)
        Log::Comment(L"WALK_TODO: requires BuildSnippetPreviewSpans helper.");
    }

    void SnippetParameterTests::Walk_Spans_TwoParams_SecondActive_FirstFilled_WALK_TODO()
    {
        // INPUT:
        //   input            = L"copy ${src} to ${dst}"
        //   parameters       = [ { Name: L"src" }, { Name: L"dst" } ]
        //   values           = { src -> L"a.txt", dst -> L"" }
        //   currentTabstop   = 1 (on `dst`)
        //
        // EXPECTED SPANS (in order):
        //   { L"copy ", Normal }
        //   { L"a.txt", Normal }       // src is FILLED but NOT active → Normal
        //   { L" to ",  Normal }
        //   { L"dst",   Placeholder }  // dst is active AND empty → Placeholder
        //
        // Rationale: only the active tabstop gets Active styling; a filled
        // non-active slot looks like prose. (Adjacent Normal runs MAY be
        // coalesced by the helper; if so, expected becomes
        // [ "copy a.txt to ", Normal ], [ "dst", Placeholder ].)
        //
        // WALK_TODO_API: BuildSnippetPreviewSpans(input, params, values, index)
        Log::Comment(L"WALK_TODO: requires BuildSnippetPreviewSpans helper.");
    }

    void SnippetParameterTests::Walk_Spans_TwoParams_FirstActive_SecondEmpty_WALK_TODO()
    {
        // INPUT:
        //   input            = L"copy ${src} to ${dst}"
        //   parameters       = [ { Name: L"src" }, { Name: L"dst" } ]
        //   values           = { src -> L"a.txt", dst -> L"" }
        //   currentTabstop   = 0 (on `src`)
        //
        // EXPECTED SPANS (in order):
        //   { L"copy ", Normal }
        //   { L"a.txt", Active }       // src is active AND typed → Active
        //   { L" to ",  Normal }
        //   { L"dst",   Placeholder }  // dst is empty (not active) → Placeholder
        //
        // Rationale: active = the tabstop currently being filled, regardless
        // of whether other slots are empty or full.
        //
        // WALK_TODO_API: BuildSnippetPreviewSpans(input, params, values, index)
        Log::Comment(L"WALK_TODO: requires BuildSnippetPreviewSpans helper.");
    }

    void SnippetParameterTests::Walk_Spans_RepeatedToken_AllOccurrencesActive_WALK_TODO()
    {
        // SPEC: Brady 2026-05-27 — multi-cursor model; all occurrences of the active tabstop render Active.
        //   Repeated ${name} behaves like a text editor with multiple
        //   cursors: even though the terminal itself doesn't have multi-
        //   cursor, editing the active tabstop mirrors the typed value
        //   live across every occurrence AND every occurrence carries the
        //   Active styling (not just the first).
        //
        // INPUT:
        //   input            = L"${x} and ${x}"
        //   parameters       = [ { Name: L"x" } ]
        //   values           = { x -> L"hi" }
        //   currentTabstop   = 0
        //
        // EXPECTED SPANS (in order):
        //   { L"hi",    Active }       // first occurrence of active tabstop
        //   { L" and ", Normal }
        //   { L"hi",    Active }       // mirror — also Active (multi-cursor model)
        //
        // WALK_TODO_API: BuildSnippetPreviewSpans(input, params, values, index)
        Log::Comment(L"WALK_TODO: requires BuildSnippetPreviewSpans helper.");
    }

    void SnippetParameterTests::Walk_Spans_EmptyParamAtNonActiveSlot_RendersPlaceholder_WALK_TODO()
    {
        // INPUT:
        //   input            = L"${a} ${b}"
        //   parameters       = [ { Name: L"a" }, { Name: L"b" } ]
        //   values           = { a -> L"", b -> L"x" }
        //   currentTabstop   = 1 (on `b`)
        //
        // EXPECTED SPANS (in order):
        //   { L"a", Placeholder }   // empty AND non-active → STILL Placeholder
        //   { L" ", Normal }
        //   { L"x", Active }        // active AND typed → Active
        //
        // Rationale: Placeholder is about value-state (empty), not about
        // active-state. An empty slot reads as a placeholder regardless of
        // which tabstop the user is currently on — this preserves the
        // user's mental model of what still needs to be filled.
        //
        // WALK_TODO_API: BuildSnippetPreviewSpans(input, params, values, index)
        Log::Comment(L"WALK_TODO: requires BuildSnippetPreviewSpans helper.");
    }

    // =====================================================================
    // 🚶 Walk-tier — UI rendering & input (D — explicit defer)
    // =====================================================================
    //
    // Per task constraint (D): anything requiring actual Composition
    // rendering, keyboard-input simulation, focus management, or cursor
    // positioning is recorded as a WALK_TODO_UI stub naming the scenario.
    // No UI automation in this round.

    void SnippetParameterTests::Walk_UI_CompositionChannel_RendersSpansToTerminal_WALK_TODO_UI()
    {
        // SCENARIO: BuildSnippetPreviewSpans output → Ripley's proposed
        // PreviewInputSpans(text, IVector<TextAttributeRun>, cursorPos) →
        // Composition.attributes → _PaintBufferOutputComposition renders
        // with the right TextAttribute per run (italic/dim for Normal +
        // Placeholder, SetReverseVideo or theme accent for Active).
        // Verify by inspecting Composition.attributes after a PreviewInput
        // round-trip.
        //
        // WALK_TODO_UI: needs ControlCore mock + Composition inspection.
        Log::Comment(L"WALK_TODO_UI: composition-channel rendering pipeline.");
    }

    void SnippetParameterTests::Walk_UI_KeystrokeUpdatesActiveTabstopLive_WALK_TODO_UI()
    {
        // SCENARIO: user types a character into the active tabstop;
        // BuildSnippetPreviewSpans is re-invoked, Composition re-rendered
        // within one frame (no stale preview). Mirrors Crawl's
        // _filterTextChanged → _previewResolvedInput → PreviewAction loop.
        //
        // WALK_TODO_UI: needs keystroke simulation + per-frame inspection.
        Log::Comment(L"WALK_TODO_UI: per-keystroke active-tabstop live update.");
    }

    void SnippetParameterTests::Walk_UI_RepeatedTokenMirrorsLiveAcrossOccurrences_WALK_TODO_UI()
    {
        // SCENARIO: snippet `${branch} && ${branch}`, user types into the
        // active tabstop, ALL occurrences update in lock-step per
        // keystroke (verifies the mirror semantic at the rendering layer,
        // not just at Resolve time).
        //
        // WALK_TODO_UI: needs keystroke simulation + multi-span inspection.
        Log::Comment(L"WALK_TODO_UI: mirror-typing on repeated ${name} renders live.");
    }

    void SnippetParameterTests::Walk_UI_CursorPositionsAtActiveTabstop_WALK_TODO_UI()
    {
        // SCENARIO: Composition.cursorPos lands at the END of the active
        // span (so the user types AT the slot, not in front of/behind it).
        // On Tab/Shift+Tab, the cursor jumps to the new active span.
        // Wide CJK chars in prefix shift the absolute column appropriately
        // (covered by existing TSF path; verify Walk doesn't regress).
        //
        // WALK_TODO_UI: needs cursorPos inspection + wide-char fixture.
        Log::Comment(L"WALK_TODO_UI: cursor positions at active tabstop on engage and Tab.");
    }

    void SnippetParameterTests::Walk_UI_EscCancelsFillAndClearsPreview_WALK_TODO_UI()
    {
        // SCENARIO: user presses Esc mid-fill. Preview Composition clears
        // (`control.PreviewInput(L"")` dismiss idiom); inline overlay
        // tears down; focus returns to the terminal; the shell sees NO
        // input (no dispatch). Matches Crawl Decision 4 (Esc = full close).
        //
        // WALK_TODO_UI: needs Esc simulation + dispatch-pipeline observation.
        Log::Comment(L"WALK_TODO_UI: Esc cancels fill and clears preview.");
    }

    void SnippetParameterTests::Walk_UI_EnterOnLastTabstopCommitsAndDispatches_WALK_TODO_UI()
    {
        // SCENARIO: user presses Enter while on the last tabstop.
        // Resolve (NOT ResolveForPreview) is invoked with the captured
        // values; the resulting hstring is sent through the normal
        // sendInput action pipeline (same SendInputArgs(resolved) shape
        // Crawl already produces). Tab-at-last has the same effect
        // (Brady-approved, see Walk_Tabstop_TabAtLastTabstopCommits_*).
        //
        // WALK_TODO_UI: needs Enter simulation + dispatch capture.
        Log::Comment(L"WALK_TODO_UI: Enter on last tabstop commits via Resolve+sendInput.");
    }

    void SnippetParameterTests::Walk_UI_TooltipShowsActiveParameterNameAndDescription_WALK_TODO_UI()
    {
        // SCENARIO: floating tooltip near the cursor shows the active
        // parameter's Name (title) and Description (body). Updates on
        // Tab/Shift+Tab. This is the spiritual successor to Crawl's
        // _descriptionsBackdrop content (Spec "What goes away" section).
        //
        // WALK_TODO_UI: needs XAML tooltip inspection.
        Log::Comment(L"WALK_TODO_UI: active-parameter tooltip content + tracking.");
    }

    void SnippetParameterTests::Walk_UI_IMECompositionPreemptsSnippetPreview_WALK_TODO_UI()
    {
        // SCENARIO (Brady-accepted as TODO): CJK user composing into a
        // parameter slot — snippetPreview and tsfPreview share
        // GetActiveComposition() so the snippet preview is pre-empted
        // during composition, reappears on commit. Test recorded so the
        // behavior is captured (not exercised — task constraint says
        // "accept and TODO; do NOT write tests that exercise this").
        //
        // WALK_TODO_UI: documents IME-collision behavior. Do not implement
        // until Brady reverses the accept-and-TODO call.
        Log::Comment(L"WALK_TODO_UI: IME composition preempts snippet preview — accepted as TODO; do not exercise.");
    }
}
