// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// SnippetParameterTests
// ---------------------
// Tests for snippet parameters on `sendInput`
// (spec: doc/specs/#1595 - Suggestions UI/Snippet Parameters.md;
//  Walk-tier addendum: doc/specs/snippet-parameters-inline-fill-addendum.md).
//
// Covers two tiers:
//
//   🐣 Crawl — settings-model contract (Parker's `SendInputArgs::Resolve`
//   + `Parameter` + load-time validation warnings).
//
//   🚶 Walk — preview-mode hooks for inline template fill:
//     * `SendInputArgs::ResolveForPreview(values, currentTabstopIndex)`
//       — empty declared parameters render as the parameter NAME.
//     * `implementation::BuildSnippetPreviewSpans(input, params, values,
//       currentTabstopIndex)` — pure free function emitting an ordered
//       (text, kind) run list ready for handoff to the spans-aware
//       preview channel.
//
// Crawl-tier contract under test:
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
// Walk-tier contract under test (Brady's load-bearing approvals):
//   * Mirror-Active scope on repeated `${name}`: multi-cursor model —
//     ALL occurrences of the active tabstop render Active (Brady
//     2026-05-27).
//   * Empty active slot renders as `Active` carrying the parameter
//     NAME (pre-selected placeholder so the user sees what's about to
//     be replaced). Non-active empty slots stay `Placeholder` (Brady
//     2026-05-27).
//   * Adjacent Normal runs ARE coalesced by BuildSnippetPreviewSpans;
//     non-active filled substitutions merge into surrounding prose.
//
// All tabstop navigation, focus, keystroke, cursor positioning, and
// Composition-channel rendering tests live in the eventual TerminalApp
// UI-test harness — they are unreachable from `UnitTests_SettingsModel`.

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
        // 🚶 Walk-tier — inline template fill at the cursor
        //
        // Tracks the Walk-tier addendum:
        //   doc/specs/snippet-parameters-inline-fill-addendum.md
        //
        // Brady's load-bearing approvals (canonical):
        //   1. Inline-template fill UX — YES
        //   2. Composition channel (PreviewInput-with-spans) — YES
        //   3. Tab at last tabstop = COMMIT, not wrap
        //   4. Mirror-Active scope on repeated `${name}`: multi-cursor
        //      model — ALL occurrences of the active tabstop render Active
        //      (Brady 2026-05-27).
        //   5. Empty active slot renders as Active carrying the parameter
        //      NAME (pre-selected placeholder) — Brady 2026-05-27.
        //      Non-active empty slots stay Placeholder.
        //
        // Helper APIs under test (now landed):
        //   * winrt::hstring SendInputArgs::ResolveForPreview(
        //         IMap<hstring,hstring> values, uint32_t currentTabstopIndex);
        //     Preview-variant of Resolve where empty declared parameters
        //     render as the parameter NAME.
        //   * std::vector<std::pair<std::wstring, SnippetPreviewSpanKind>>
        //     implementation::BuildSnippetPreviewSpans(
        //         std::wstring_view input,
        //         IVectorView<Parameter> parameters,
        //         IMap<hstring,hstring> values,
        //         uint32_t currentTabstopIndex);
        //     Pure free function in `implementation::` namespace; adjacent
        //     Normal runs are coalesced.
        //   * enum SnippetPreviewSpanKind { Normal = 0, Active, Placeholder }.
        // -----------------------------------------------------------------

        // (A) Settings-model layer — preview-mode hook
        TEST_METHOD(Walk_ResolveForPreview_EmptyValueRendersParameterName);
        TEST_METHOD(Walk_ResolveForPreview_TypedValueRendersValue);
        TEST_METHOD(Walk_ResolveForPreview_RepeatedTokenMirrorsValue);
        TEST_METHOD(Walk_ResolveForPreview_UndeclaredTokenPassesThrough);

        // (C) Span construction for the inline preview — model-layer logic
        TEST_METHOD(Walk_Spans_SingleParam_EmptyValue_ActiveIndex);
        TEST_METHOD(Walk_Spans_SingleParam_TypedValue_ActiveIndex);
        TEST_METHOD(Walk_Spans_SingleParam_TypedValue_PastLastIndex);
        TEST_METHOD(Walk_Spans_TwoParams_SecondActive_FirstFilled);
        TEST_METHOD(Walk_Spans_TwoParams_FirstActive_SecondEmpty);
        TEST_METHOD(Walk_Spans_RepeatedToken_AllOccurrencesActive);
        TEST_METHOD(Walk_Spans_EmptyParamAtNonActiveSlot_RendersPlaceholder);

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
    // 🚶 Walk-tier — model-layer preview hook (A)
    // =====================================================================
    //
    // `SendInputArgs::ResolveForPreview` mirrors `Resolve` but renders
    // empty-valued *declared* parameters as the parameter NAME (placeholder)
    // instead of the empty string. This is the semantic the inline preview
    // needs so the user can SEE what they're filling in before they start
    // typing (fixes Brady's "the thing I'm filling disappears" complaint).
    //
    // The dispatch-time Resolve semantics (empty → empty) are unchanged —
    // already covered by SnippetParameter_ResolveEmptyValueAllowed above.

    void SnippetParameterTests::Walk_ResolveForPreview_EmptyValueRendersParameterName()
    {
        // input          = "git checkout ${branch}"
        // parameters     = [ branch ]
        // values         = { branch -> "" }
        // currentTabstop = 0
        // Expected: declared-but-empty parameter renders as its NAME.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "git checkout ${branch}",
            "parameters": [ { "name": "branch" } ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"branch", L"" } });
        const auto resolved = args->ResolveForPreview(values, 0);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"git checkout branch" }, resolved);
    }

    void SnippetParameterTests::Walk_ResolveForPreview_TypedValueRendersValue()
    {
        // Once the user has typed anything, preview mode behaves exactly
        // like Resolve — placeholder semantics only apply to the empty case.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "git checkout ${branch}",
            "parameters": [ { "name": "branch" } ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"branch", L"main" } });
        const auto resolved = args->ResolveForPreview(values, 0);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"git checkout main" }, resolved);
    }

    void SnippetParameterTests::Walk_ResolveForPreview_RepeatedTokenMirrorsValue()
    {
        // Mirror typing — both occurrences of ${branch} render the typed
        // value. Same Resolve semantic verified at dispatch by
        // SnippetParameter_ResolveRepeatedOccurrence; this asserts
        // ResolveForPreview honors it too.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "git checkout -b ${branch} && git push -u origin ${branch}",
            "parameters": [ { "name": "branch" } ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"branch", L"feature/x" } });
        const auto resolved = args->ResolveForPreview(values, 0);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"git checkout -b feature/x && git push -u origin feature/x" }, resolved);
    }

    void SnippetParameterTests::Walk_ResolveForPreview_UndeclaredTokenPassesThrough()
    {
        // Undeclared tokens are NOT tabstops; they render literally in the
        // preview. Matches dispatch-time Resolve behavior (see
        // SnippetParameter_ResolveUndeclaredTokenPassesThrough). Spec §6.5:
        // "Undeclared ${foo} in input is NOT a tabstop."
        const auto args = makeSendInputArgs(L"echo ${foo}");
        const auto values = makeValueMap({});
        const auto resolved = args->ResolveForPreview(values, 0);
        VERIFY_ARE_EQUAL(winrt::hstring{ L"echo ${foo}" }, resolved);
    }

    // =====================================================================
    // 🚶 Walk-tier — span construction for inline preview (C)
    // =====================================================================
    //
    // `implementation::BuildSnippetPreviewSpans` is a pure free function
    //   (input, parameters, values, currentTabstopIndex) -> vector<(text, kind)>
    // Three kinds (SnippetPreviewSpanKind, IDL ordering Normal=0/Active/Placeholder):
    //   Normal      — prose, or a typed mirror of a non-active filled slot
    //                 (coalesced into surrounding prose).
    //   Placeholder — empty value at a NON-active slot — renders the
    //                 parameter NAME (faint).
    //   Active      — active tabstop, every occurrence (multi-cursor model).
    //                 Carries the typed VALUE when non-empty, or the
    //                 parameter NAME when empty (pre-selected placeholder,
    //                 Brady 2026-05-27).
    //
    // Adjacent Normal runs ARE coalesced (the helper's `pendingNormal`
    // buffer flushes on each non-Normal emission and at end-of-input).
    // Non-active filled substitutions are appended to that buffer — they
    // merge with surrounding prose into one Normal span.

    void SnippetParameterTests::Walk_Spans_SingleParam_EmptyValue_ActiveIndex()
    {
        // input          = "git checkout ${branch}"
        // parameters     = [ branch ]
        // values         = { branch -> "" }
        // currentTabstop = 0  (branch is ACTIVE)
        // Expected per the four-cell contract (Brady 2026-05-27):
        //   empty + active -> Active span carrying the NAME.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "git checkout ${branch}",
            "parameters": [ { "name": "branch" } ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"branch", L"" } });
        const auto spans = implementation::BuildSnippetPreviewSpans(
            std::wstring_view{ args->Input() }, args->Parameters().GetView(), values, 0);

        VERIFY_ARE_EQUAL(2u, spans.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"git checkout " }, spans[0].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Normal, spans[0].second);
        VERIFY_ARE_EQUAL(std::wstring{ L"branch" }, spans[1].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Active, spans[1].second);
    }

    void SnippetParameterTests::Walk_Spans_SingleParam_TypedValue_ActiveIndex()
    {
        // Typed value at the active tabstop -> Active styling carrying VALUE.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "git checkout ${branch}",
            "parameters": [ { "name": "branch" } ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"branch", L"main" } });
        const auto spans = implementation::BuildSnippetPreviewSpans(
            std::wstring_view{ args->Input() }, args->Parameters().GetView(), values, 0);

        VERIFY_ARE_EQUAL(2u, spans.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"git checkout " }, spans[0].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Normal, spans[0].second);
        VERIFY_ARE_EQUAL(std::wstring{ L"main" }, spans[1].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Active, spans[1].second);
    }

    void SnippetParameterTests::Walk_Spans_SingleParam_TypedValue_PastLastIndex()
    {
        // currentTabstop = 1 with paramCount = 1 -> out of range, no active
        // tabstop. The filled value is appended to pendingNormal and merges
        // with surrounding prose into ONE coalesced Normal span.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "git checkout ${branch}",
            "parameters": [ { "name": "branch" } ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"branch", L"main" } });
        const auto spans = implementation::BuildSnippetPreviewSpans(
            std::wstring_view{ args->Input() }, args->Parameters().GetView(), values, 1);

        VERIFY_ARE_EQUAL(1u, spans.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"git checkout main" }, spans[0].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Normal, spans[0].second);
    }

    void SnippetParameterTests::Walk_Spans_TwoParams_SecondActive_FirstFilled()
    {
        // input          = "copy ${src} to ${dst}"
        // parameters     = [ src, dst ]
        // values         = { src -> "a.txt", dst -> "" }
        // currentTabstop = 1 (on `dst`)
        //
        // `src` is filled and NOT active -> coalesces into surrounding prose.
        // `dst` is active and empty -> Active span carrying the NAME.
        //
        // Adjacent Normal coalescing collapses ["copy ", "a.txt", " to "]
        // into a single ("copy a.txt to ", Normal) span.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "copy ${src} to ${dst}",
            "parameters":
            [
                { "name": "src" },
                { "name": "dst" }
            ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"src", L"a.txt" }, { L"dst", L"" } });
        const auto spans = implementation::BuildSnippetPreviewSpans(
            std::wstring_view{ args->Input() }, args->Parameters().GetView(), values, 1);

        VERIFY_ARE_EQUAL(2u, spans.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"copy a.txt to " }, spans[0].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Normal, spans[0].second);
        VERIFY_ARE_EQUAL(std::wstring{ L"dst" }, spans[1].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Active, spans[1].second);
    }

    void SnippetParameterTests::Walk_Spans_TwoParams_FirstActive_SecondEmpty()
    {
        // input          = "copy ${src} to ${dst}"
        // parameters     = [ src, dst ]
        // values         = { src -> "a.txt", dst -> "" }
        // currentTabstop = 0 (on `src`)
        //
        // `src` is active AND typed -> Active span carrying the VALUE.
        // `dst` is empty AND non-active -> Placeholder span carrying NAME.
        // No coalescing opportunity — each Active/Placeholder boundary
        // flushes pendingNormal.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "copy ${src} to ${dst}",
            "parameters":
            [
                { "name": "src" },
                { "name": "dst" }
            ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"src", L"a.txt" }, { L"dst", L"" } });
        const auto spans = implementation::BuildSnippetPreviewSpans(
            std::wstring_view{ args->Input() }, args->Parameters().GetView(), values, 0);

        VERIFY_ARE_EQUAL(4u, spans.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"copy " }, spans[0].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Normal, spans[0].second);
        VERIFY_ARE_EQUAL(std::wstring{ L"a.txt" }, spans[1].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Active, spans[1].second);
        VERIFY_ARE_EQUAL(std::wstring{ L" to " }, spans[2].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Normal, spans[2].second);
        VERIFY_ARE_EQUAL(std::wstring{ L"dst" }, spans[3].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Placeholder, spans[3].second);
    }

    void SnippetParameterTests::Walk_Spans_RepeatedToken_AllOccurrencesActive()
    {
        // SPEC: Brady 2026-05-27 — multi-cursor model. Repeated ${name}
        // behaves like a text editor with multiple cursors: every
        // occurrence of the active tabstop carries Active styling.
        //
        // input          = "${x} and ${x}"
        // parameters     = [ x ]
        // values         = { x -> "hi" }
        // currentTabstop = 0
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "${x} and ${x}",
            "parameters": [ { "name": "x" } ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"x", L"hi" } });
        const auto spans = implementation::BuildSnippetPreviewSpans(
            std::wstring_view{ args->Input() }, args->Parameters().GetView(), values, 0);

        VERIFY_ARE_EQUAL(3u, spans.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"hi" }, spans[0].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Active, spans[0].second);
        VERIFY_ARE_EQUAL(std::wstring{ L" and " }, spans[1].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Normal, spans[1].second);
        VERIFY_ARE_EQUAL(std::wstring{ L"hi" }, spans[2].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Active, spans[2].second);
    }

    void SnippetParameterTests::Walk_Spans_EmptyParamAtNonActiveSlot_RendersPlaceholder()
    {
        // input          = "${a} ${b}"
        // parameters     = [ a, b ]
        // values         = { a -> "", b -> "x" }
        // currentTabstop = 1 (on `b`)
        //
        // `a` is empty AND non-active -> Placeholder span carrying NAME.
        // `b` is active AND typed -> Active span carrying VALUE.
        // Preserves the user's mental model of what still needs filling.
        static constexpr std::string_view json{ R"(
        {
            "action": "sendInput",
            "input": "${a} ${b}",
            "parameters":
            [
                { "name": "a" },
                { "name": "b" }
            ]
        })" };
        auto [args, warnings] = parseSendInputArgsFromActionJson(json);
        const auto values = makeValueMap({ { L"a", L"" }, { L"b", L"x" } });
        const auto spans = implementation::BuildSnippetPreviewSpans(
            std::wstring_view{ args->Input() }, args->Parameters().GetView(), values, 1);

        VERIFY_ARE_EQUAL(3u, spans.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"a" }, spans[0].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Placeholder, spans[0].second);
        VERIFY_ARE_EQUAL(std::wstring{ L" " }, spans[1].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Normal, spans[1].second);
        VERIFY_ARE_EQUAL(std::wstring{ L"x" }, spans[2].first);
        VERIFY_ARE_EQUAL(SnippetPreviewSpanKind::Active, spans[2].second);
    }
}
