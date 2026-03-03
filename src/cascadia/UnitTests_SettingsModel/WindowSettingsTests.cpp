// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/WindowSettings.h"
#include "../TerminalSettingsModel/resource.h"
#include "JsonTestClass.h"
#include "TestUtils.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace SettingsModelUnitTests
{
    class WindowSettingsTests : public JsonTestClass
    {
        TEST_CLASS(WindowSettingsTests);

        // === Deserialization tests ===
        TEST_METHOD(DefaultQuakeWindowSettings);
        TEST_METHOD(LayeredWindowSettings);
        TEST_METHOD(LayeredOnDefaultWindowSettings);
        TEST_METHOD(LayeredQuakeWindowSettings);
        TEST_METHOD(TestGeneratedQuakeWindowSettings);

        // === Edge-case tests ===
        TEST_METHOD(NoWindowsDefined);
        TEST_METHOD(LookupUnknownWindowName);
        TEST_METHOD(InheritedSettingsFromDefaults);
        TEST_METHOD(NamedWindowOverridesDefaults);
        TEST_METHOD(PartialNamedWindowInheritsRest);
        TEST_METHOD(MultipleNamedWindows);

        // === Serialization roundtrip ===
        TEST_METHOD(RoundtripWindowSettings);
        TEST_METHOD(RoundtripNamedWindows);

    private:
        // Helper: build a CascadiaSettings from a user-settings JSON string,
        // with a minimal inbox that provides at least one profile and the
        // Campbell colour scheme.
        static winrt::com_ptr<implementation::CascadiaSettings> createSettings(const std::string_view& userJSON)
        {
            static constexpr std::string_view inboxJSON{ R"({
                "profiles": [
                    {
                        "name": "Default",
                        "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                        "commandline": "cmd.exe"
                    }
                ],
                "schemes": [
                    {
                        "name": "Campbell",
                        "foreground": "#CCCCCC",
                        "background": "#0C0C0C",
                        "cursorColor": "#FFFFFF",
                        "black": "#0C0C0C",
                        "red": "#C50F1F",
                        "green": "#13A10E",
                        "yellow": "#C19C00",
                        "blue": "#0037DA",
                        "purple": "#881798",
                        "cyan": "#3A96DD",
                        "white": "#CCCCCC",
                        "brightBlack": "#767676",
                        "brightRed": "#E74856",
                        "brightGreen": "#16C60C",
                        "brightYellow": "#F9F1A5",
                        "brightBlue": "#3B78FF",
                        "brightPurple": "#B4009E",
                        "brightCyan": "#61D6D6",
                        "brightWhite": "#F2F2F2"
                    }
                ]
            })" };

            return winrt::make_self<implementation::CascadiaSettings>(userJSON, inboxJSON);
        }
    };

    // ===================================================================
    // Deserialization tests
    // ===================================================================

    // Verify that asking for the _quake window when no explicit quake
    // settings are defined still produces reasonable quake defaults.
    void WindowSettingsTests::DefaultQuakeWindowSettings()
    {
        Log::Comment(L"When no windows are defined, requesting '_quake' "
                     L"should return quake-mode defaults (focus mode, dock top, etc.).");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ]
        })" };

        const auto settings = createSettings(settingsJson);

        // The _quake window is synthesised on-the-fly when not explicitly
        // defined in the "windows" array.
        const auto quake = settings->WindowSettings(L"_quake");

        VERIFY_ARE_EQUAL(LaunchMode::FocusMode, quake.LaunchMode());
        VERIFY_IS_NOT_NULL(quake.DockWindow());
        VERIFY_ARE_EQUAL(DockPosition::Top, quake.DockWindow().Side());
        VERIFY_ARE_EQUAL(1.0, quake.DockWindow().Width());
        VERIFY_ARE_EQUAL(0.5, quake.DockWindow().Height());
        VERIFY_IS_TRUE(quake.MinimizeToNotificationArea());
    }

    // Verify that a named window declared in the "windows" array gets its
    // explicit settings, and inherits everything else from the defaults.
    void WindowSettingsTests::LayeredWindowSettings()
    {
        Log::Comment(L"A named window should get its own explicit values and "
                     L"inherit everything else from the base window settings.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialRows": 50,
            "initialCols": 100,
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ],
            "windows": [
                {
                    "name": "work",
                    "initialRows": 80,
                    "copyOnSelect": true
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto work = settings->WindowSettings(L"work");

        // Explicit overrides
        VERIFY_ARE_EQUAL(80, work.InitialRows());
        VERIFY_IS_TRUE(work.CopyOnSelect());

        // Inherited from base (which was explicitly set to 100)
        VERIFY_ARE_EQUAL(100, work.InitialCols());
    }

    // Verify that a named window that only specifies a name still inherits
    // every setting from the base window defaults.
    void WindowSettingsTests::LayeredOnDefaultWindowSettings()
    {
        Log::Comment(L"A named window with no explicit settings should be "
                     L"identical to the base window defaults.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialRows": 42,
            "copyOnSelect": true,
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ],
            "windows": [
                {
                    "name": "empty"
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto empty = settings->WindowSettings(L"empty");
        const auto defaults = settings->WindowSettingsDefaults();

        VERIFY_ARE_EQUAL(defaults.InitialRows(), empty.InitialRows());
        VERIFY_ARE_EQUAL(defaults.InitialCols(), empty.InitialCols());
        VERIFY_ARE_EQUAL(defaults.CopyOnSelect(), empty.CopyOnSelect());
        VERIFY_ARE_EQUAL(defaults.WordDelimiters(), empty.WordDelimiters());
    }

    // Verify that an explicitly defined _quake window in the "windows" array
    // gets its custom values while still starting from quake defaults.
    void WindowSettingsTests::LayeredQuakeWindowSettings()
    {
        Log::Comment(L"An explicitly-defined _quake window should honour its "
                     L"custom values but still start from quake defaults.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ],
            "windows": [
                {
                    "name": "_quake",
                    "initialRows": 20
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto quake = settings->WindowSettings(L"_quake");

        // Custom override
        VERIFY_ARE_EQUAL(20, quake.InitialRows());

        // Quake defaults should still apply (InitializeForQuakeMode is called
        // by LayerJson when name == "_quake").
        VERIFY_ARE_EQUAL(LaunchMode::FocusMode, quake.LaunchMode());
        VERIFY_IS_NOT_NULL(quake.DockWindow());
        VERIFY_ARE_EQUAL(DockPosition::Top, quake.DockWindow().Side());
        VERIFY_IS_TRUE(quake.MinimizeToNotificationArea());
    }

    // Verify that asking for _quake when no windows are defined at all still
    // generates a valid quake window with inheritance from the base.
    void WindowSettingsTests::TestGeneratedQuakeWindowSettings()
    {
        Log::Comment(L"Generated _quake settings should still inherit from "
                     L"the base window defaults.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialRows": 99,
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto quake = settings->WindowSettings(L"_quake");

        // The generated quake window should inherit the base window's rows
        // BEFORE InitializeForQuakeMode overwrites launch mode / docking.
        // Actually, InitializeForQuakeMode does not touch InitialRows, so
        // the inherited value should come through.
        VERIFY_ARE_EQUAL(99, quake.InitialRows());

        // Quake-specific defaults
        VERIFY_ARE_EQUAL(LaunchMode::FocusMode, quake.LaunchMode());
        VERIFY_IS_NOT_NULL(quake.DockWindow());
        VERIFY_ARE_EQUAL(DockPosition::Top, quake.DockWindow().Side());
    }

    // ===================================================================
    // Edge-case tests
    // ===================================================================

    // When no "windows" array is present, WindowSettingsDefaults() should
    // still return a valid object and WindowSettings(name) should fall back
    // to the base defaults.
    void WindowSettingsTests::NoWindowsDefined()
    {
        Log::Comment(L"Without a 'windows' array, WindowSettingsDefaults() "
                     L"must still be valid with standard defaults.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto defaults = settings->WindowSettingsDefaults();

        // The defaults should have the standard built-in values.
        VERIFY_ARE_EQUAL(30, defaults.InitialRows());
        VERIFY_ARE_EQUAL(80, defaults.InitialCols());
        VERIFY_IS_FALSE(defaults.CopyOnSelect());
        VERIFY_IS_NULL(defaults.DockWindow());
        VERIFY_IS_FALSE(defaults.MinimizeToNotificationArea());

        // AllWindowSettings should be empty — no named windows exist.
        VERIFY_ARE_EQUAL(0u, settings->AllWindowSettings().Size());
    }

    // Looking up a window name that doesn't exist in the "windows" array
    // should fall back to the base window defaults rather than crashing.
    void WindowSettingsTests::LookupUnknownWindowName()
    {
        Log::Comment(L"Requesting a window name that doesn't exist should "
                     L"return the base window defaults.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialRows": 45,
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ],
            "windows": [
                {
                    "name": "known",
                    "initialRows": 60
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);

        // "unknown" is NOT in the windows list.
        const auto unknown = settings->WindowSettings(L"unknown");
        const auto defaults = settings->WindowSettingsDefaults();

        // Should be identical to the base defaults (initialRows 45).
        VERIFY_ARE_EQUAL(defaults.InitialRows(), unknown.InitialRows());
        VERIFY_ARE_EQUAL(45, unknown.InitialRows());

        // Meanwhile the "known" window should have its override.
        const auto known = settings->WindowSettings(L"known");
        VERIFY_ARE_EQUAL(60, known.InitialRows());
    }

    // When the base window settings override a default and a named window
    // does NOT explicitly set that property, the named window should inherit
    // the base value (not the hard-coded MTSM default).
    void WindowSettingsTests::InheritedSettingsFromDefaults()
    {
        Log::Comment(L"A named window should inherit the user's root-level "
                     L"overrides, not the hard-coded defaults.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialRows": 70,
            "initialCols": 200,
            "copyOnSelect": true,
            "wordDelimiters": "abc",
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ],
            "windows": [
                {
                    "name": "child"
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto child = settings->WindowSettings(L"child");

        // All values should come from the base window settings (user root).
        VERIFY_ARE_EQUAL(70, child.InitialRows());
        VERIFY_ARE_EQUAL(200, child.InitialCols());
        VERIFY_IS_TRUE(child.CopyOnSelect());
        VERIFY_ARE_EQUAL(L"abc", child.WordDelimiters());
    }

    // When a named window explicitly sets a property, it should override the
    // base defaults.
    void WindowSettingsTests::NamedWindowOverridesDefaults()
    {
        Log::Comment(L"Named-window explicit values must override the base defaults.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialRows": 30,
            "copyOnSelect": false,
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ],
            "windows": [
                {
                    "name": "override",
                    "initialRows": 100,
                    "copyOnSelect": true
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto win = settings->WindowSettings(L"override");

        VERIFY_ARE_EQUAL(100, win.InitialRows());
        VERIFY_IS_TRUE(win.CopyOnSelect());

        // Base defaults should be unchanged.
        const auto defaults = settings->WindowSettingsDefaults();
        VERIFY_ARE_EQUAL(30, defaults.InitialRows());
        VERIFY_IS_FALSE(defaults.CopyOnSelect());
    }

    // A named window that specifies only some properties should inherit the
    // rest from the base defaults.
    void WindowSettingsTests::PartialNamedWindowInheritsRest()
    {
        Log::Comment(L"A partially-specified named window inherits the "
                     L"remaining properties from base window defaults.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialRows": 35,
            "initialCols": 120,
            "wordDelimiters": "xyz",
            "alwaysShowTabs": false,
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ],
            "windows": [
                {
                    "name": "partial",
                    "initialRows": 50
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto partial = settings->WindowSettings(L"partial");

        // Overridden
        VERIFY_ARE_EQUAL(50, partial.InitialRows());

        // Inherited from base
        VERIFY_ARE_EQUAL(120, partial.InitialCols());
        VERIFY_ARE_EQUAL(L"xyz", partial.WordDelimiters());
        VERIFY_IS_FALSE(partial.AlwaysShowTabs());
    }

    // Verify that multiple named windows each get their own values and don't
    // bleed into each other.
    void WindowSettingsTests::MultipleNamedWindows()
    {
        Log::Comment(L"Multiple named windows must each get their own overrides "
                     L"without affecting each other.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialRows": 30,
            "initialCols": 80,
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ],
            "windows": [
                {
                    "name": "alpha",
                    "initialRows": 40,
                    "copyOnSelect": true
                },
                {
                    "name": "beta",
                    "initialCols": 200,
                    "alwaysOnTop": true
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto alpha = settings->WindowSettings(L"alpha");
        const auto beta = settings->WindowSettings(L"beta");
        const auto defaults = settings->WindowSettingsDefaults();

        // Alpha overrides
        VERIFY_ARE_EQUAL(40, alpha.InitialRows());
        VERIFY_IS_TRUE(alpha.CopyOnSelect());
        // Alpha inherits cols from base
        VERIFY_ARE_EQUAL(80, alpha.InitialCols());
        // Alpha should NOT have beta's alwaysOnTop
        VERIFY_IS_FALSE(alpha.AlwaysOnTop());

        // Beta overrides
        VERIFY_ARE_EQUAL(200, beta.InitialCols());
        VERIFY_IS_TRUE(beta.AlwaysOnTop());
        // Beta inherits rows from base
        VERIFY_ARE_EQUAL(30, beta.InitialRows());
        // Beta should NOT have alpha's copyOnSelect
        VERIFY_IS_FALSE(beta.CopyOnSelect());

        // We defined two named windows.
        VERIFY_ARE_EQUAL(2u, settings->AllWindowSettings().Size());
    }

    // ===================================================================
    // Serialization roundtrip tests
    // ===================================================================

    // Verify that WindowSettings survive a FromJson -> ToJson roundtrip.
    void WindowSettingsTests::RoundtripWindowSettings()
    {
        Log::Comment(L"WindowSettings should roundtrip through JSON correctly.");

        static constexpr std::string_view windowJson{ R"({
            "copyOnSelect": true,
            "initialCols": 200,
            "initialRows": 50,
            "wordDelimiters": "test"
        })" };

        const auto json = VerifyParseSucceeded(windowJson);
        const auto windowSettings = implementation::WindowSettings::FromJson(json);
        const auto result = windowSettings->ToJson();

        // The roundtrip should produce the same keys and values.
        VERIFY_IS_TRUE(result.isMember("copyOnSelect"));
        VERIFY_IS_TRUE(result["copyOnSelect"].asBool());
        VERIFY_ARE_EQUAL(200, result["initialCols"].asInt());
        VERIFY_ARE_EQUAL(50, result["initialRows"].asInt());
        VERIFY_ARE_EQUAL(std::string("test"), result["wordDelimiters"].asString());
    }

    // Verify that a full CascadiaSettings with named windows roundtrips
    // correctly through ToJson.
    void WindowSettingsTests::RoundtripNamedWindows()
    {
        Log::Comment(L"Named windows in 'windows' array should survive a "
                     L"settings roundtrip.");

        static constexpr std::string_view settingsJson{ R"({
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialRows": 42,
            "profiles": [
                { "name": "profile0", "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" }
            ],
            "windows": [
                {
                    "name": "myWindow",
                    "initialRows": 60,
                    "copyOnSelect": true
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        const auto json = settings->ToJson();

        // The root should have our base initialRows.
        VERIFY_ARE_EQUAL(42, json["initialRows"].asInt());

        // The "windows" array should exist and contain our named window.
        VERIFY_IS_TRUE(json.isMember("windows"));
        const auto& windows = json["windows"];
        VERIFY_ARE_EQUAL(1u, windows.size());

        const auto& win = windows[0u];
        VERIFY_ARE_EQUAL(std::string("myWindow"), win["name"].asString());
        VERIFY_ARE_EQUAL(60, win["initialRows"].asInt());
        VERIFY_IS_TRUE(win["copyOnSelect"].asBool());
    }
}
