// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "JsonTestClass.h"
#include "TestUtils.h"
#include <defaults.h>
#include "../ut_app/TestDynamicProfileGenerator.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace SettingsModelLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class SerializationTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(SerializationTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(GlobalSettings);
        TEST_METHOD(Profile);
        TEST_METHOD(ColorScheme);
        TEST_METHOD(CascadiaSettings);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            InitializeJsonWriter();
            return true;
        }

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
        void RoundtripTest(const std::string& jsonString)
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
    };

    void SerializationTests::GlobalSettings()
    {
        const std::string globalsString{ R"(
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

                "startOnUserLogin": false,
                "theme": "system",
                "snapToGridOnResize": true,
                "disableAnimations": false,

                "confirmCloseAllTabs": true,
                "largePasteWarning": true,
                "multiLinePasteWarning": true,

                "experimental.input.forceVT": false,
                "experimental.rendering.forceFullRepaint": false,
                "experimental.rendering.software": false
            })" };

        const std::string smallGlobalsString{ R"(
            {
                "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}"
            })" };

        RoundtripTest<implementation::GlobalAppSettings>(globalsString);
        RoundtripTest<implementation::GlobalAppSettings>(smallGlobalsString);
    }

    void SerializationTests::Profile()
    {
        const std::string profileString{ R"(
            {
                "name": "Windows PowerShell",
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",

                "commandline": "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
                "startingDirectory": "%USERPROFILE%",

                "icon": "ms-appx:///ProfileIcons/{61c54bbd-c2c6-5271-96e7-009a87ff44bf}.png",
                "hidden": false,

                "tabTitle": "Cool Tab",
                "suppressApplicationTitle": false,

                "fontFace": "Cascadia Mono",
                "fontSize": 12,
                "fontWeight": "normal",
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
                "acrylicOpacity": 0.5,

                "backgroundImage": "made_you_look.jpeg",
                "backgroundImageStretchMode": "uniformToFill",
                "backgroundImageAlignment": "center",
                "backgroundImageOpacity": 1.0,

                "scrollbarState": "visible",
                "snapOnInput": true,
                "historySize": 9001,

                "closeOnExit": "graceful",
                "experimental.retroTerminalEffect": false
            })" };

        const std::string smallProfileString{ R"(
            {
                "name": "Custom Profile"
            })" };

        // Setting "tabColor" to null tests two things:
        // - null should count as an explicit user-set value, not falling back to the parent's value
        // - null should be acceptable even though we're working with colors
        const std::string weirdProfileString{ R"(
            {
                "name": "Weird Profile",
                "tabColor": null,
                "foreground": null,
                "source": "local"
            })" };

        RoundtripTest<implementation::Profile>(profileString);
        RoundtripTest<implementation::Profile>(smallProfileString);
        RoundtripTest<implementation::Profile>(weirdProfileString);
    }

    void SerializationTests::ColorScheme()
    {
        const std::string schemeString{ R"({
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

    void SerializationTests::CascadiaSettings()
    {
        const std::string settingsString{ R"({
                                                "$schema": "https://aka.ms/terminal-profiles-schema",
                                                "defaultProfile": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",

                                                "profiles": {
                                                    "defaults": {
                                                        "fontFace": "Zamora Code"
                                                    },
                                                    "list": [
                                                        {
                                                            "fontFace": "Cascadia Code",
                                                            "guid": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
                                                            "name": "HowettShell"
                                                        },
                                                        {
                                                            "hidden": true,
                                                            "name": "BhojwaniShell"
                                                        },
                                                        {
                                                            "antialiasingMode": "aliased",
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
                                                    {"command": { "action": "renameTab","input": "Liang Tab" },"keys": "ctrl+t" }
                                                ],
                                                "keybindings": [
                                                    { "command": { "action": "sendInput","input": "VT Griese Mode" },"keys": "ctrl+k" }
                                                ]
                                            })" };

        auto settings{ winrt::make_self<implementation::CascadiaSettings>(false) };
        settings->_ParseJsonString(settingsString, false);
        settings->_ApplyDefaultsFromUserSettings();
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        const auto result{ settings->ToJson() };
        VERIFY_ARE_EQUAL(toString(settings->_userSettings), toString(result));
    }
}
