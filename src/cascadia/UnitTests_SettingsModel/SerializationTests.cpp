// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/resource.h"
#include "JsonTestClass.h"
#include "TestUtils.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

// Different architectures will hash the same SendInput command to a different ID
// Check for the correct ID based on the architecture
#if defined(_M_IX86)
#define SEND_INPUT_ARCH_SPECIFIC_ACTION_HASH "56911147"
#else
#define SEND_INPUT_ARCH_SPECIFIC_ACTION_HASH "A020D2"
#endif

#if defined(_M_IX86)
#define SEND_INPUT2_ARCH_SPECIFIC_ACTION_HASH "35488AA6"
#else
#define SEND_INPUT2_ARCH_SPECIFIC_ACTION_HASH "58D1971"
#endif

namespace SettingsModelUnitTests
{
    class SerializationTests : public JsonTestClass
    {
        TEST_CLASS(SerializationTests);

        TEST_METHOD(GlobalSettings);
        TEST_METHOD(Profile);
        TEST_METHOD(ColorScheme);
        TEST_METHOD(Actions);
        TEST_METHOD(CascadiaSettings);
        TEST_METHOD(LegacyFontSettings);

        TEST_METHOD(RoundtripReloadEnvVars);
        TEST_METHOD(DontRoundtripNoReloadEnvVars);

        TEST_METHOD(RoundtripUserModifiedColorSchemeCollision);
        TEST_METHOD(RoundtripUserModifiedColorSchemeCollisionUnusedByProfiles);
        TEST_METHOD(RoundtripUserDeletedColorSchemeCollision);

        TEST_METHOD(RoundtripGenerateActionID);
        TEST_METHOD(NoGeneratedIDsForIterableAndNestedCommands);
        TEST_METHOD(GeneratedActionIDsEqualForIdenticalCommands);
        TEST_METHOD(RoundtripLegacyToModernActions);
        TEST_METHOD(RoundtripUserActionsSameAsInBoxAreRemoved);
        TEST_METHOD(RoundtripActionsSameNameDifferentCommandsAreRetained);
        TEST_METHOD(MultipleActionsAreCollapsed);

    private:
        // Method Description:
        // - deserializes and reserializes a json string representing a settings object model of type T
        // - verifies that the generated json string matches the provided one
        // Template Types:
        // - <T>: The type of Settings Model object to generate (must be impl type)
        // Arguments:
        // - jsonString - JSON string we're performing the test on
        // Return Value:
        // - the JsonObject representing this instance
        template<typename T>
        void RoundtripTest(const std::string_view& jsonString)
        {
            const auto json{ VerifyParseSucceeded(jsonString) };
            const auto settings{ T::FromJson(json) };
            const auto result{ settings->ToJson() };

            // Compare toString(json) instead of jsonString here.
            // The toString writes the json out alphabetically.
            // This trick allows jsonString to _not_ have to be
            // written alphabetically.
            VERIFY_ARE_EQUAL(toString(json), toString(result));
        }

        // Helper to remove the `$schema` property from a json object. We
        // populate that based off the local path to the settings file. Of
        // course, that's entirely unpredictable in tests. So cut it out before
        // we do any sort of roundtrip testing.
        static Json::Value removeSchema(Json::Value json)
        {
            if (json.isMember("$schema"))
            {
                json.removeMember("$schema");
            }
            return json;
        }
    };

    void SerializationTests::GlobalSettings()
    {
        static constexpr std::string_view globalsString{ R"(
            {
                "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",

                "initialRows": 30,
                "initialCols": 120,
                "initialPosition": ",",
                "launchMode": "default",
                "alwaysOnTop": false,
                "copyOnSelect": false,
                "copyFormatting": "all",
                "wordDelimiters": " /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502",

                "alwaysShowTabs": true,
                "showTabsInTitlebar": true,
                "showTerminalTitleInTitlebar": true,
                "tabWidthMode": "equal",
                "tabSwitcherMode": "mru",

                "theme": "system",
                "snapToGridOnResize": true,
                "disableAnimations": false,

                "trimPaste": true,

                "warning.confirmCloseAllTabs" : true,
                "warning.inputService" : true,
                "warning.largePaste" : true,
                "warning.multiLinePaste" : true,

                "actions": [],
                "keybindings": []
            })" };

        static constexpr std::string_view smallGlobalsString{ R"(
            {
                "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "actions": [],
                "keybindings": []
            })" };

        RoundtripTest<implementation::GlobalAppSettings>(globalsString);
        RoundtripTest<implementation::GlobalAppSettings>(smallGlobalsString);
    }

    void SerializationTests::Profile()
    {
        static constexpr std::string_view profileString{ R"(
            {
                "name": "Windows PowerShell",
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",

                "commandline": "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
                "startingDirectory": "%USERPROFILE%",

                "icon": "ms-appx:///ProfileIcons/{61c54bbd-c2c6-5271-96e7-009a87ff44bf}.png",
                "hidden": false,

                "tabTitle": "Cool Tab",
                "suppressApplicationTitle": false,

                "font": {
                    "face": "Cascadia Mono",
                    "size": 12,
                    "weight": "normal"
                },
                "padding": "8, 8, 8, 8",
                "antialiasingMode": "grayscale",

                "cursorShape": "bar",
                "cursorColor": "#CCBBAA",
                "cursorHeight": 10,

                "altGrAliasing": true,

                "colorScheme": "Campbell",
                "tabColor": "#0C0C0C",
                "foreground": "#AABBCC",
                "background": "#BBCCAA",
                "selectionBackground": "#CCAABB",

                "useAcrylic": false,
                "opacity": 50,

                "backgroundImage": "made_you_look.jpeg",
                "backgroundImageStretchMode": "uniformToFill",
                "backgroundImageAlignment": "center",
                "backgroundImageOpacity": 1,

                "scrollbarState": "visible",
                "snapOnInput": true,
                "historySize": 9001,

                "closeOnExit": "graceful",
                "experimental.retroTerminalEffect": false,
                "environment":
                {
                    "KEY_1": "VALUE_1",
                    "KEY_2": "%KEY_1%",
                    "KEY_3": "%PATH%"
                }
            })" };

        static constexpr std::string_view smallProfileString{ R"(
            {
                "name": "Custom Profile"
            })" };

        // Setting "tabColor" to null tests two things:
        // - null should count as an explicit user-set value, not falling back to the parent's value
        // - null should be acceptable even though we're working with colors
        static constexpr std::string_view weirdProfileString{ R"(
            {
                "guid" : "{8b039d4d-77ca-5a83-88e1-dfc8e895a127}",
                "name": "Weird Profile",
                "hidden": false,
                "tabColor": null,
                "foreground": null,
                "source": "local"
            })" };

        static constexpr std::string_view profileWithIcon{ R"(
            {
                "guid" : "{8b039d4d-77ca-5a83-88e1-dfc8e895a127}",
                "name": "profileWithIcon",
                "hidden": false,
                "icon": "foo.png"
            })" };
        static constexpr std::string_view profileWithNullIcon{ R"(
            {
                "guid" : "{8b039d4d-77ca-5a83-88e1-dfc8e895a127}",
                "name": "profileWithNullIcon",
                "hidden": false,
                "icon": null
            })" };
        static constexpr std::string_view profileWithNoIcon{ R"(
            {
                "guid" : "{8b039d4d-77ca-5a83-88e1-dfc8e895a127}",
                "name": "profileWithNoIcon",
                "hidden": false,
                "icon": "none"
            })" };

        RoundtripTest<implementation::Profile>(profileString);
        RoundtripTest<implementation::Profile>(smallProfileString);
        RoundtripTest<implementation::Profile>(weirdProfileString);
        RoundtripTest<implementation::Profile>(profileWithIcon);
        RoundtripTest<implementation::Profile>(profileWithNullIcon);
        RoundtripTest<implementation::Profile>(profileWithNoIcon);
    }

    void SerializationTests::ColorScheme()
    {
        static constexpr std::string_view schemeString{ R"({
                                            "name": "Campbell",

                                            "cursorColor": "#FFFFFF",
                                            "selectionBackground": "#131313",

                                            "background": "#0C0C0C",
                                            "foreground": "#F2F2F2",

                                            "black": "#0C0C0C",
                                            "blue": "#0037DA",
                                            "cyan": "#3A96DD",
                                            "green": "#13A10E",
                                            "purple": "#881798",
                                            "red": "#C50F1F",
                                            "white": "#CCCCCC",
                                            "yellow": "#C19C00",
                                            "brightBlack": "#767676",
                                            "brightBlue": "#3B78FF",
                                            "brightCyan": "#61D6D6",
                                            "brightGreen": "#16C60C",
                                            "brightPurple": "#B4009E",
                                            "brightRed": "#E74856",
                                            "brightWhite": "#F2F2F2",
                                            "brightYellow": "#F9F1A5"
                                        })" };

        RoundtripTest<implementation::ColorScheme>(schemeString);
    }

    void SerializationTests::Actions()
    {
        // simple command
        static constexpr std::string_view actionsString1{ R"([
                                                { "command": "paste", "id": "Test.Paste" }
                                            ])" };

        // complex command
        static constexpr std::string_view actionsString2A{ R"([
                                                { "command": { "action": "setTabColor" }, "id": "Test.SetTabColor" }
                                            ])" };
        static constexpr std::string_view actionsString2B{ R"([
                                                { "command": { "action": "setTabColor", "color": "#112233" }, "id": "Test.SetTabColor112233" }
                                            ])" };
        static constexpr std::string_view actionsString2C{ R"([
                                                { "command": { "action": "copy" }, "id": "Test.Copy" },
                                                { "command": { "action": "copy", "singleLine": true, "copyFormatting": "html" }, "id": "Test.CopyWithArgs" }
                                            ])" };

        // simple command with key chords
        static constexpr std::string_view actionsString3{ R"({ "actions": [
                                                { "command": "toggleAlwaysOnTop", "id": "Test.ToggleAlwaysOnTop" } ],
                                            "keybindings": [
                                                { "keys": "ctrl+a", "id": "Test.ToggleAlwaysOnTop" },
                                                { "keys": "ctrl+b", "id": "Test.ToggleAlwaysOnTop" } ]})" };

        // complex command with key chords
        static constexpr std::string_view actionsString4A{ R"({ "actions":[
                                                { "command": { "action": "adjustFontSize", "delta": 1 }, "id": "Test.EnlargeFont" } ],
                                            "keybindings": [
                                                { "keys": "ctrl+c", "id": "Test.EnlargeFont" },
                                                { "keys": "ctrl+d", "id": "Test.EnlargeFont" } ]})" };

        // command with name and icon and multiple key chords
        static constexpr std::string_view actionsString5{ R"({ "actions":[
                                                { "icon": "image.png", "name": "Scroll To Top Name", "command": "scrollToTop", "id": "Test.ScrollToTop" } ],
                                            "keybindings": [
                                                { "id": "Test.ScrollToTop", "keys": "ctrl+f" },
                                                { "id": "Test.ScrollToTop", "keys": "ctrl+e" } ]})" };

        // complex command with new terminal args
        static constexpr std::string_view actionsString6{ R"([
                                                { "command": { "action": "newTab", "index": 0 }, "id": "Test.NewTerminal" },
                                            ])" };

        // complex command with meaningful null arg
        static constexpr std::string_view actionsString7{ R"([
                                                { "command": { "action": "renameWindow", "name": null }, "id": "Test.MeaningfulNull" }
                                            ])" };

        // nested command
        static constexpr std::string_view actionsString8{ R"([
                                                {
                                                    "name": "Change font size...",
                                                    "commands": [
                                                        { "command": { "action": "adjustFontSize", "delta": 1 } },
                                                        { "command": { "action": "adjustFontSize", "delta": -1 } },
                                                        { "command": "resetFontSize" },
                                                    ]
                                                }
                                            ])" };

        // iterable command
        static constexpr std::string_view actionsString9A{ R"([
                                                {
                                                    "name": "New tab",
                                                    "commands": [
                                                        {
                                                            "iterateOn": "profiles",
                                                            "icon": "${profile.icon}",
                                                            "name": "${profile.name}",
                                                            "command": { "action": "newTab", "profile": "${profile.name}" }
                                                        }
                                                    ]
                                                }
                                            ])" };
        static constexpr std::string_view actionsString9B{ R"([
                                                {
                                                    "commands":
                                                    [
                                                        {
                                                            "command":
                                                            {
                                                                "action": "sendInput",
                                                                "input": "${profile.name}"
                                                            },
                                                            "iterateOn": "profiles"
                                                        }
                                                    ],
                                                    "name": "Send Input ..."
                                                }
                                        ])" };
        static constexpr std::string_view actionsString9C{ R""([
                                                {
                                                    "commands":
                                                    [
                                                        {
                                                            "commands":
                                                            [
                                                                {
                                                                    "command":
                                                                    {
                                                                        "action": "sendInput",
                                                                        "input": "${profile.name} ${scheme.name}"
                                                                    },
                                                                    "iterateOn": "schemes"
                                                                }
                                                            ],
                                                            "iterateOn": "profiles",
                                                            "name": "nest level (${profile.name})"
                                                        }
                                                    ],
                                                    "name": "Send Input (Evil) ..."
                                                }
                                            ])"" };
        static constexpr std::string_view actionsString9D{ R""([
                                                {
                                                    "command":
                                                    {
                                                        "action": "newTab",
                                                        "profile": "${profile.name}"
                                                    },
                                                    "icon": "${profile.icon}",
                                                    "iterateOn": "profiles",
                                                    "name": "${profile.name}: New tab"
                                                }
                                            ])"" };

        // unbound command
        static constexpr std::string_view actionsString10{ R"({ "actions": [],
                                            "keybindings": [
                                                { "id": null, "keys": "ctrl+c" } ]})" };

        Log::Comment(L"simple command");
        RoundtripTest<implementation::ActionMap>(actionsString1);

        Log::Comment(L"complex commands");
        RoundtripTest<implementation::ActionMap>(actionsString2A);
        RoundtripTest<implementation::ActionMap>(actionsString2B);
        RoundtripTest<implementation::ActionMap>(actionsString2C);

        // ActionMap has effectively 2 "to json" calls we need to make, one for the actions and one for the keybindings
        // So we cannot use RoundtripTest<ActionMap> for actions + keychords, just use RoundTripTest<GlobalAppSettings>
        Log::Comment(L"simple command with key chords");
        RoundtripTest<implementation::GlobalAppSettings>(actionsString3);

        Log::Comment(L"complex commands with key chords");
        RoundtripTest<implementation::GlobalAppSettings>(actionsString4A);

        Log::Comment(L"command with name and icon and multiple key chords");
        RoundtripTest<implementation::GlobalAppSettings>(actionsString5);

        Log::Comment(L"complex command with new terminal args");
        RoundtripTest<implementation::ActionMap>(actionsString6);

        Log::Comment(L"complex command with meaningful null arg");
        RoundtripTest<implementation::ActionMap>(actionsString7);

        Log::Comment(L"nested command");
        RoundtripTest<implementation::ActionMap>(actionsString8);

        Log::Comment(L"iterable command");
        RoundtripTest<implementation::ActionMap>(actionsString9A);
        RoundtripTest<implementation::ActionMap>(actionsString9B);
        RoundtripTest<implementation::ActionMap>(actionsString9C);
        RoundtripTest<implementation::ActionMap>(actionsString9D);

        Log::Comment(L"unbound command");
        RoundtripTest<implementation::GlobalAppSettings>(actionsString10);
    }

    void SerializationTests::CascadiaSettings()
    {
        static constexpr std::string_view settingsString{ R"({
            "$help" : "https://aka.ms/terminal-documentation",
            "$schema" : "https://aka.ms/terminal-profiles-schema",
            "defaultProfile": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
            "disabledProfileSources": [ "Windows.Terminal.Wsl" ],
            "newTabMenu":
            [
                {
                    "type": "remainingProfiles"
                }
            ],
            "profiles": {
                "defaults": {
                    "font": {
                        "face": "Zamora Code"
                    }
                },
                "list": [
                    {
                        "font": { "face": "Cascadia Code" },
                        "guid": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
                        "name": "HowettShell"
                    },
                    {
                        "hidden": true,
                        "guid": "{c08b0496-e71c-5503-b84e-3af7a7a6d2a7}",
                        "name": "BhojwaniShell"
                    },
                    {
                        "antialiasingMode": "aliased",
                        "guid": "{fe9df758-ac22-5c20-922d-c7766cdd13af}",
                        "name": "NiksaShell"
                    }
                ]
            },
            "schemes": [
                {
                    "name": "Cinnamon Roll",

                    "cursorColor": "#FFFFFD",
                    "selectionBackground": "#FFFFFF",

                    "background": "#3C0315",
                    "foreground": "#FFFFFD",

                    "black": "#282A2E",
                    "blue": "#0170C5",
                    "cyan": "#3F8D83",
                    "green": "#76AB23",
                    "purple": "#7D498F",
                    "red": "#BD0940",
                    "white": "#FFFFFD",
                    "yellow": "#E0DE48",
                    "brightBlack": "#676E7A",
                    "brightBlue": "#5C98C5",
                    "brightCyan": "#8ABEB7",
                    "brightGreen": "#B5D680",
                    "brightPurple": "#AC79BB",
                    "brightRed": "#BD6D85",
                    "brightWhite": "#FFFFFD",
                    "brightYellow": "#FFFD76"
                }
            ],
            "actions": [
                { "command": { "action": "sendInput", "input": "VT Griese Mode" }, "id": "Test.SendInput" }
            ],
            "keybindings": [
                { "id": "Test.SendInput", "keys": "ctrl+k" }
            ],
            "theme": "system",
            "themes": []
        })" };

        const auto settings{ winrt::make_self<implementation::CascadiaSettings>(settingsString) };

        const auto result{ settings->ToJson() };
        VERIFY_ARE_EQUAL(toString(removeSchema(VerifyParseSucceeded(settingsString))),
                         toString(removeSchema(result)));
    }

    void SerializationTests::LegacyFontSettings()
    {
        static constexpr std::string_view profileString{ R"(
            {
                "name": "Profile with legacy font settings",

                "fontFace": "Cascadia Mono",
                "fontSize": 12,
                "fontWeight": "normal"
            })" };

        static constexpr std::string_view expectedOutput{ R"(
            {
                "name": "Profile with legacy font settings",

                "font": {
                    "face": "Cascadia Mono",
                    "size": 12,
                    "weight": "normal"
                }
            })" };

        const auto json{ VerifyParseSucceeded(profileString) };
        const auto settings{ implementation::Profile::FromJson(json) };
        const auto result{ settings->ToJson() };

        const auto jsonOutput{ VerifyParseSucceeded(expectedOutput) };

        VERIFY_ARE_EQUAL(toString(jsonOutput), toString(result));
    }

    void SerializationTests::RoundtripReloadEnvVars()
    {
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "compatibility.reloadEnvironmentVariables": false,
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                }
            ],
            "actions": [
                {
                    "name": "foo",
                    "command": "closePane",
                    "keys": "ctrl+shift+w"
                }
            ]
        })" };

        static constexpr std::string_view newSettingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles":
            {
                "defaults":
                {
                    "compatibility.reloadEnvironmentVariables": false
                },
                "list":
                [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "historySize": 1,
                        "commandline": "cmd.exe"
                    }
                ]
            },
            "actions": [
                {
                    "name": "foo",
                    "command": "closePane",
                    "keys": "ctrl+shift+w"
                }
            ]
        })" };

        implementation::SettingsLoader oldLoader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        oldLoader.MergeInboxIntoUserSettings();
        oldLoader.FinalizeLayering();
        VERIFY_IS_TRUE(oldLoader.FixupUserSettings(), L"Validate that this will indicate we need to write them back to disk");
        const auto oldSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(oldLoader));
        const auto oldResult{ oldSettings->ToJson() };

        implementation::SettingsLoader newLoader{ newSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        newLoader.FixupUserSettings();
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));
        const auto newResult{ newSettings->ToJson() };

        VERIFY_ARE_EQUAL(toString(newResult), toString(oldResult));
    }

    void SerializationTests::DontRoundtripNoReloadEnvVars()
    {
        // Kinda like the above test, but confirming that _nothing_ happens if
        // we don't have a setting to migrate.

        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                }
            ],
            "actions": [
                {
                    "name": "foo",
                    "command": "closePane",
                    "keys": "ctrl+shift+w"
                }
            ]
        })" };

        implementation::SettingsLoader oldLoader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        oldLoader.MergeInboxIntoUserSettings();
        oldLoader.FinalizeLayering();
        oldLoader.FixupUserSettings();
        const auto oldSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(oldLoader));
        const auto oldResult{ oldSettings->ToJson() };

        Log::Comment(L"Now, create a _new_ settings object from the re-serialization of the first");
        implementation::SettingsLoader newLoader{ toString(oldResult), implementation::LoadStringResource(IDR_DEFAULTS) };
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        newLoader.FixupUserSettings();
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));
        VERIFY_IS_FALSE(newSettings->ProfileDefaults().HasReloadEnvironmentVariables(),
                        L"Ensure that the new settings object didn't find a reloadEnvironmentVariables");
    }

    void SerializationTests::RoundtripUserModifiedColorSchemeCollision()
    {
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name": "profile1",
                    "colorScheme": "Tango Dark",
                    "guid": "{d0a65a9d-8665-4128-97a4-a581aa747aa7}"
                }
            ],
            "schemes": [
                {
                    "background": "#121314",
                    "black": "#121314",
                    "blue": "#121314",
                    "brightBlack": "#121314",
                    "brightBlue": "#121314",
                    "brightCyan": "#121314",
                    "brightGreen": "#121314",
                    "brightPurple": "#121314",
                    "brightRed": "#121314",
                    "brightWhite": "#121314",
                    "brightYellow": "#121314",
                    "cursorColor": "#121314",
                    "cyan": "#121314",
                    "foreground": "#121314",
                    "green": "#121314",
                    "name": "Campbell",
                    "purple": "#121314",
                    "red": "#121314",
                    "selectionBackground": "#121314",
                    "white": "#121314",
                    "yellow": "#121314"
                },
                {
                    "background": "#000000",
                    "black": "#000000",
                    "blue": "#3465A4",
                    "brightBlack": "#555753",
                    "brightBlue": "#729FCF",
                    "brightCyan": "#34E2E2",
                    "brightGreen": "#8AE234",
                    "brightPurple": "#AD7FA8",
                    "brightRed": "#EF2929",
                    "brightWhite": "#EEEEEC",
                    "brightYellow": "#FCE94F",
                    "cursorColor": "#FFFFFF",
                    "cyan": "#06989A",
                    "foreground": "#D3D7CF",
                    "green": "#4E9A06",
                    "name": "Tango Dark",
                    "purple": "#75507B",
                    "red": "#CC0000",
                    "selectionBackground": "#FFFFFF",
                    "white": "#D3D7CF",
                    "yellow": "#C4A000"
                },
            ]
        })" };

        // Key differences: one fewer color scheme (Tango Dark has been deleted) and defaults.colorScheme is set.
        static constexpr std::string_view newSettingsJson{ R"-(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles":
            {
                "defaults": {
                    "colorScheme": "Campbell (modified)"
                },
                "list":
                [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                    },
                    {
                        "name": "profile1",
                        "colorScheme": "Tango Dark",
                        "guid": "{d0a65a9d-8665-4128-97a4-a581aa747aa7}"
                    }
                ]
            },
            "actions": [ ],
            "schemes": [
                {
                    "background": "#121314",
                    "black": "#121314",
                    "blue": "#121314",
                    "brightBlack": "#121314",
                    "brightBlue": "#121314",
                    "brightCyan": "#121314",
                    "brightGreen": "#121314",
                    "brightPurple": "#121314",
                    "brightRed": "#121314",
                    "brightWhite": "#121314",
                    "brightYellow": "#121314",
                    "cursorColor": "#121314",
                    "cyan": "#121314",
                    "foreground": "#121314",
                    "green": "#121314",
                    "name": "Campbell",
                    "purple": "#121314",
                    "red": "#121314",
                    "selectionBackground": "#121314",
                    "white": "#121314",
                    "yellow": "#121314"
                }
            ]
        })-" };

        implementation::SettingsLoader oldLoader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        oldLoader.MergeInboxIntoUserSettings();
        oldLoader.FinalizeLayering();
        VERIFY_IS_TRUE(oldLoader.FixupUserSettings(), L"Validate that this will indicate we need to write them back to disk");
        const auto oldSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(oldLoader));
        const auto oldResult{ oldSettings->ToJson() };

        implementation::SettingsLoader newLoader{ newSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        newLoader.FixupUserSettings();
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));
        const auto newResult{ newSettings->ToJson() };

        VERIFY_ARE_EQUAL(toString(newResult), toString(oldResult));
    }

    void SerializationTests::RoundtripUserModifiedColorSchemeCollisionUnusedByProfiles()
    {
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                }
            ],
            "schemes": [
                {
                    "background": "#111111",
                    "black": "#111111",
                    "blue": "#111111",
                    "brightBlack": "#111111",
                    "brightBlue": "#111111",
                    "brightCyan": "#111111",
                    "brightGreen": "#111111",
                    "brightPurple": "#111111",
                    "brightRed": "#111111",
                    "brightWhite": "#111111",
                    "brightYellow": "#111111",
                    "cursorColor": "#111111",
                    "cyan": "#111111",
                    "foreground": "#111111",
                    "green": "#111111",
                    "name": "Tango Dark",
                    "purple": "#111111",
                    "red": "#111111",
                    "selectionBackground": "#111111",
                    "white": "#111111",
                    "yellow": "#111111"
                },
            ]
        })" };

        // Key differences: Tango Dark has been renamed; nothing else has changed
        static constexpr std::string_view newSettingsJson{ R"-(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles":
            {
                "list":
                [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                    }
                ]
            },
            "actions": [ ],
            "schemes": [
                {
                    "background": "#111111",
                    "black": "#111111",
                    "blue": "#111111",
                    "brightBlack": "#111111",
                    "brightBlue": "#111111",
                    "brightCyan": "#111111",
                    "brightGreen": "#111111",
                    "brightPurple": "#111111",
                    "brightRed": "#111111",
                    "brightWhite": "#111111",
                    "brightYellow": "#111111",
                    "cursorColor": "#111111",
                    "cyan": "#111111",
                    "foreground": "#111111",
                    "green": "#111111",
                    "name": "Tango Dark (modified)",
                    "purple": "#111111",
                    "red": "#111111",
                    "selectionBackground": "#111111",
                    "white": "#111111",
                    "yellow": "#111111"
                },
            ]
        })-" };

        implementation::SettingsLoader oldLoader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        oldLoader.MergeInboxIntoUserSettings();
        oldLoader.FinalizeLayering();
        VERIFY_IS_TRUE(oldLoader.FixupUserSettings(), L"Validate that this will indicate we need to write them back to disk");
        const auto oldSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(oldLoader));
        const auto oldResult{ oldSettings->ToJson() };

        implementation::SettingsLoader newLoader{ newSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        newLoader.FixupUserSettings();
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));
        const auto newResult{ newSettings->ToJson() };

        VERIFY_ARE_EQUAL(toString(newResult), toString(oldResult));
    }

    void SerializationTests::RoundtripUserDeletedColorSchemeCollision()
    {
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                }
            ],
            "schemes": [
                {
                    "name": "Tango Dark",
                    "foreground": "#D3D7CF",
                    "background": "#000000",
                    "cursorColor": "#FFFFFF",
                    "black": "#000000",
                    "red": "#CC0000",
                    "green": "#4E9A06",
                    "yellow": "#C4A000",
                    "blue": "#3465A4",
                    "purple": "#75507B",
                    "cyan": "#06989A",
                    "white": "#D3D7CF",
                    "brightBlack": "#555753",
                    "brightRed": "#EF2929",
                    "brightGreen": "#8AE234",
                    "brightYellow": "#FCE94F",
                    "brightBlue": "#729FCF",
                    "brightPurple": "#AD7FA8",
                    "brightCyan": "#34E2E2",
                    "brightWhite": "#EEEEEC"
                }
            ]
        })" };

        // Key differences: Tango Dark has been deleted, as it was identical to the inbox one.
        static constexpr std::string_view newSettingsJson{ R"-(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles":
            {
                "list":
                [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                    }
                ]
            },
            "actions": [ ],
            "schemes": [ ]
        })-" };

        implementation::SettingsLoader oldLoader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        oldLoader.MergeInboxIntoUserSettings();
        oldLoader.FinalizeLayering();
        VERIFY_IS_TRUE(oldLoader.FixupUserSettings(), L"Validate that this will indicate we need to write them back to disk");
        const auto oldSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(oldLoader));
        const auto oldResult{ oldSettings->ToJson() };

        implementation::SettingsLoader newLoader{ newSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        newLoader.FixupUserSettings();
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));
        const auto newResult{ newSettings->ToJson() };

        VERIFY_ARE_EQUAL(toString(newResult), toString(oldResult));
    }

    void SerializationTests::RoundtripGenerateActionID()
    {
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "actions": [
                {
                    "name": "foo",
                    "command": { "action": "sendInput", "input": "just some input" },
                    "keys": "ctrl+shift+w"
                }
            ]
        })" };

        implementation::SettingsLoader loader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        loader.MergeInboxIntoUserSettings();
        loader.FinalizeLayering();
        loader.FixupUserSettings();
        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));
        const auto oldResult{ settings->ToJson() };
        const auto sendInputCmd = settings->ActionMap().GetActionByKeyChord(KeyChord{ true, false, true, false, 87, 0 });

        std::string_view expectedID{ R"(User.sendInput.)" SEND_INPUT_ARCH_SPECIFIC_ACTION_HASH };

        VERIFY_ARE_EQUAL(sendInputCmd.ID(), winrt::to_hstring(expectedID));
    }

    void SerializationTests::NoGeneratedIDsForIterableAndNestedCommands()
    {
        // for iterable commands, nested commands, and user-defined actions that already have
        // an ID, we do not need to generate an ID
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "actions": [
                {
                    "name": "foo",
                    "command": "closePane",
                    "id": "thisIsMyClosePane"
                },
                {
                    "iterateOn": "profiles",
                    "icon": "${profile.icon}",
                    "name": "${profile.name}",
                    "command": { "action": "newTab", "profile": "${profile.name}" }
                },
                {
                    "name": "Change font size...",
                    "commands": [
                        { "command": { "action": "adjustFontSize", "delta": 1 } },
                        { "command": { "action": "adjustFontSize", "delta": -1 } },
                        { "command": "resetFontSize" },
                    ]
                }
            ]
        })" };

        implementation::SettingsLoader oldLoader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        oldLoader.MergeInboxIntoUserSettings();
        oldLoader.FinalizeLayering();
        VERIFY_IS_FALSE(oldLoader.FixupUserSettings(), L"Validate that there is no need to write back to disk");
    }

    void SerializationTests::GeneratedActionIDsEqualForIdenticalCommands()
    {
        static constexpr std::string_view settingsJson1{ R"(
        {
            "actions": [
                {
                    "name": "foo",
                    "command": { "action": "sendInput", "input": "this is some other input string" },
                    "keys": "ctrl+shift+w"
                }
            ]
        })" };

        // Both settings files define the same action, so the generated ID should be the same for both
        static constexpr std::string_view settingsJson2{ R"(
        {
            "actions": [
                {
                    "name": "foo",
                    "command": { "action": "sendInput", "input": "this is some other input string" },
                    "keys": "ctrl+shift+w"
                }
            ]
        })" };

        implementation::SettingsLoader loader1{ settingsJson1, implementation::LoadStringResource(IDR_DEFAULTS) };
        loader1.MergeInboxIntoUserSettings();
        loader1.FinalizeLayering();
        loader1.FixupUserSettings();
        const auto settings1 = winrt::make_self<implementation::CascadiaSettings>(std::move(loader1));
        const auto result1{ settings1->ToJson() };

        implementation::SettingsLoader loader2{ settingsJson2, implementation::LoadStringResource(IDR_DEFAULTS) };
        loader2.MergeInboxIntoUserSettings();
        loader2.FinalizeLayering();
        loader2.FixupUserSettings();
        const auto settings2 = winrt::make_self<implementation::CascadiaSettings>(std::move(loader2));
        const auto result2{ settings2->ToJson() };

        const auto sendInputCmd1 = settings1->ActionMap().GetActionByKeyChord(KeyChord{ true, false, true, false, 87, 0 });
        const auto sendInputCmd2 = settings2->ActionMap().GetActionByKeyChord(KeyChord{ true, false, true, false, 87, 0 });

        VERIFY_ARE_EQUAL(sendInputCmd1.ID(), sendInputCmd1.ID());
    }

    void SerializationTests::RoundtripLegacyToModernActions()
    {
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "actions": [
                {
                    "name": "foo",
                    "id": "Test.SendInput",
                    "command": { "action": "sendInput", "input": "just some input" },
                    "keys": "ctrl+shift+w"
                },
                {
                    "command": "unbound",
                    "keys": "ctrl+shift+x"
                }
            ]
        })" };

        // modern style:
        // - no "unbound" actions, these are just keybindings that have no id
        // - no keys in actions, these are keybindings with an id
        static constexpr std::string_view newSettingsJson{ R"(
        {
            "actions": [
                {
                    "name": "foo",
                    "command": { "action": "sendInput", "input": "just some input" },
                    "id": "Test.SendInput"
                }
            ],
            "keybindings": [
                {
                    "id": "Test.SendInput",
                    "keys": "ctrl+shift+w"
                },
                {
                    "id": null,
                    "keys": "ctrl+shift+x"
                }
            ]
        })" };

        implementation::SettingsLoader loader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        loader.MergeInboxIntoUserSettings();
        loader.FinalizeLayering();
        VERIFY_IS_TRUE(loader.FixupUserSettings(), L"Validate that this will indicate we need to write them back to disk");
        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));
        const auto oldResult{ settings->ToJson() };

        implementation::SettingsLoader newLoader{ newSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        VERIFY_IS_FALSE(newLoader.FixupUserSettings(), L"Validate that there is no need to write back to disk");
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));
        const auto newResult{ newSettings->ToJson() };

        VERIFY_ARE_EQUAL(toString(newResult), toString(oldResult));
    }

    void SerializationTests::RoundtripUserActionsSameAsInBoxAreRemoved()
    {
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "actions": [
                {
                    "command": "paste",
                    "keys": "ctrl+shift+x"
                }
            ]
        })" };

        // this action is the same as in inbox one,
        // so we will delete this action from the user's file but retain the keybinding
        static constexpr std::string_view newSettingsJson{ R"(
        {
            "actions": [
            ],
            "keybindings": [
                {
                    "id": "Terminal.PasteFromClipboard",
                    "keys": "ctrl+shift+x"
                }
            ]
        })" };

        implementation::SettingsLoader loader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        loader.MergeInboxIntoUserSettings();
        loader.FinalizeLayering();
        VERIFY_IS_TRUE(loader.FixupUserSettings(), L"Validate that this will indicate we need to write them back to disk");
        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));
        const auto oldResult{ settings->ToJson() };

        implementation::SettingsLoader newLoader{ newSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        VERIFY_IS_FALSE(newLoader.FixupUserSettings(), L"Validate that there is no need to write back to disk");
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));
        const auto newResult{ newSettings->ToJson() };

        VERIFY_ARE_EQUAL(toString(newResult), toString(oldResult));
    }

    void SerializationTests::RoundtripActionsSameNameDifferentCommandsAreRetained()
    {
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "actions": [
                {
                    "command": { "action": "sendInput", "input": "just some input" },
                    "name": "mySendInput"
                },
                {
                    "command": { "action": "sendInput", "input": "just some input 2" },
                    "name": "mySendInput"
                }
            ]
        })" };

        // There are two different actions with the same name,
        // ensure that both are kept but have different IDs generated for them
        static constexpr std::string_view newSettingsJson{ R"(
        {
            "actions": [
                {
                    "name": "mySendInput",
                    "command": { "action": "sendInput", "input": "just some input" },
                    "id": "User.sendInput.)" SEND_INPUT_ARCH_SPECIFIC_ACTION_HASH R"("
                },
                {
                    "name": "mySendInput",
                    "command": { "action": "sendInput", "input": "just some input 2" },
                    "id": "User.sendInput.)" SEND_INPUT2_ARCH_SPECIFIC_ACTION_HASH R"("
                }
            ]
        })" };

        implementation::SettingsLoader loader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        loader.MergeInboxIntoUserSettings();
        loader.FinalizeLayering();
        VERIFY_IS_TRUE(loader.FixupUserSettings(), L"Validate that this will indicate we need to write them back to disk");
        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));
        const auto oldResult{ settings->ToJson() };

        implementation::SettingsLoader newLoader{ newSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        VERIFY_IS_FALSE(newLoader.FixupUserSettings(), L"Validate that there is no need to write back to disk");
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));
        const auto newResult{ newSettings->ToJson() };

        VERIFY_ARE_EQUAL(toString(newResult), toString(oldResult));
    }

    void SerializationTests::MultipleActionsAreCollapsed()
    {
        static constexpr std::string_view oldSettingsJson{ R"(
        {
            "actions": [
                {
                    "name": "foo",
                    "icon": "myCoolIconPath.png",
                    "command": { "action": "sendInput", "input": "just some input" },
                    "keys": "ctrl+shift+w"
                },
                {
                    "command": { "action": "sendInput", "input": "just some input" },
                    "keys": "ctrl+shift+x"
                }
            ]
        })" };

        // modern style:
        // - multiple action blocks whose purpose is simply to define more keybindings for the same action
        //   get collapsed into one action block, with the name and icon path preserved and have multiple keybindings instead
        static constexpr std::string_view newSettingsJson{ R"(
        {
            "actions": [
                {
                    "name": "foo",
                    "icon": "myCoolIconPath.png",
                    "command": { "action": "sendInput", "input": "just some input" },
                    "id": "User.sendInput.)" SEND_INPUT_ARCH_SPECIFIC_ACTION_HASH R"("
                }
            ],
            "keybindings": [
                {
                    "keys": "ctrl+shift+w",
                    "id": "User.sendInput.)" SEND_INPUT_ARCH_SPECIFIC_ACTION_HASH R"("
                },
                {
                    "keys": "ctrl+shift+x",
                    "id": "User.sendInput.)" SEND_INPUT_ARCH_SPECIFIC_ACTION_HASH R"("
                }
            ]
        })" };

        implementation::SettingsLoader loader{ oldSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        loader.MergeInboxIntoUserSettings();
        loader.FinalizeLayering();
        VERIFY_IS_TRUE(loader.FixupUserSettings(), L"Validate that this will indicate we need to write them back to disk");
        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));
        const auto oldResult{ settings->ToJson() };

        implementation::SettingsLoader newLoader{ newSettingsJson, implementation::LoadStringResource(IDR_DEFAULTS) };
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        VERIFY_IS_FALSE(newLoader.FixupUserSettings(), L"Validate that there is no need to write back to disk");
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));
        const auto newResult{ newSettings->ToJson() };

        VERIFY_ARE_EQUAL(toString(newResult), toString(oldResult));
    }
}
