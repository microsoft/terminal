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
            return true;
        }

    private:
        std::string toString(const Json::Value& json)
        {
            // set indentation to empty string to remove newlines
            // enableYAMLCompatibility adds a space after ':'
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "";
            builder["enableYAMLCompatibility"] = true;
            return Json::writeString(builder, json);
        }
    };

    void SerializationTests::GlobalSettings()
    {
        // This needs to be in alphabetical order.
        const std::string globalsString{
            "{"
            "\"alwaysOnTop\": false,"
            "\"alwaysShowTabs\": true,"
            "\"confirmCloseAllTabs\": true,"
            "\"copyFormatting\": \"all\","
            "\"copyOnSelect\": false,"
            "\"defaultProfile\": \"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}\","
            "\"experimental.input.forceVT\": false,"
            "\"experimental.rendering.forceFullRepaint\": false,"
            "\"experimental.rendering.software\": false,"
            "\"initialCols\": 120,"
            "\"initialPosition\": \",\","
            "\"initialRows\": 30,"
            "\"largePasteWarning\": true,"
            "\"launchMode\": \"default\","
            "\"multiLinePasteWarning\": true,"
            "\"showTabsInTitlebar\": true,"
            "\"showTerminalTitleInTitlebar\": true,"
            "\"snapToGridOnResize\": true,"
            "\"startOnUserLogin\": false,"
            "\"tabWidthMode\": \"equal\","
            "\"theme\": \"system\","
            "\"useTabSwitcher\": true,"
            "\"wordDelimiters\": \" /\\\\()\\\"'-.,:;<>~!@#$%^&*|+=[]{}~?\\u2502\""
            "}"
        };

        const std::string smallGlobalsString{
            "{"
            "\"defaultProfile\": \"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}\""
            "}"
        };

        auto roundtripTest = [this](auto jsonString) {
            const auto json{ VerifyParseSucceeded(jsonString) };
            const auto globals{ implementation::GlobalAppSettings::FromJson(json) };

            const auto result{ globals->ToJson() };

            VERIFY_ARE_EQUAL(jsonString, toString(result));
        };

        roundtripTest(globalsString);
        roundtripTest(smallGlobalsString);
    }

    void SerializationTests::Profile()
    {
        // This needs to be in alphabetical order.
        const std::string profileString{
            "{"
            "\"altGrAliasing\": true,"
            "\"antialiasingMode\": \"grayscale\","
            "\"background\": \"#BBCCAA\","
            "\"backgroundImage\": \"made_you_look.jpeg\","
            "\"backgroundImageAlignment\": \"center\","
            "\"backgroundImageOpacity\": 1.0,"
            "\"backgroundImageStretchMode\": \"uniformToFill\","
            "\"closeOnExit\": \"graceful\","
            "\"colorScheme\": \"Campbell\","
            "\"commandline\": \"%SystemRoot%\\\\System32\\\\WindowsPowerShell\\\\v1.0\\\\powershell.exe\","
            "\"cursorColor\": \"#CCBBAA\","
            "\"cursorHeight\": 10,"
            "\"cursorShape\": \"bar\","
            "\"experimental.retroTerminalEffect\": false,"
            "\"fontFace\": \"Cascadia Mono\","
            "\"fontSize\": 12,"
            "\"fontWeight\": \"normal\","
            "\"foreground\": \"#AABBCC\","
            "\"guid\": \"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}\","
            "\"hidden\": false,"
            "\"historySize\": 9001,"
            "\"icon\": \"ms-appx:///ProfileIcons/{61c54bbd-c2c6-5271-96e7-009a87ff44bf}.png\","
            "\"name\": \"Windows PowerShell\","
            "\"padding\": \"8, 8, 8, 8\","
            "\"scrollbarState\": \"visible\","
            "\"selectionBackground\": \"#CCAABB\","
            "\"snapOnInput\": true,"
            "\"startingDirectory\": \"%USERPROFILE%\","
            "\"suppressApplicationTitle\": false,"
            "\"tabColor\": \"#0C0C0C\","
            "\"tabTitle\": \"Cool Tab\","
            "\"useAcrylic\": false"
            "}"
        };

        const std::string smallProfileString{
            "{"
            "\"name\": \"Custom Profile\""
            "}"
        };

        auto roundtripTest = [this](auto jsonString) {
            const auto json{ VerifyParseSucceeded(jsonString) };
            const auto profile{ implementation::Profile::FromJson(json) };

            const auto result{ profile->ToJson() };

            VERIFY_ARE_EQUAL(jsonString, toString(result));
        };

        roundtripTest(profileString);
        roundtripTest(smallProfileString);
    }

    void SerializationTests::ColorScheme()
    {
        // This needs to be in alphabetical order.
        const std::string schemeString{ "{"
                                        "\"background\": \"#0C0C0C\","
                                        "\"black\": \"#0C0C0C\","
                                        "\"blue\": \"#0037DA\","
                                        "\"brightBlack\": \"#767676\","
                                        "\"brightBlue\": \"#3B78FF\","
                                        "\"brightCyan\": \"#61D6D6\","
                                        "\"brightGreen\": \"#16C60C\","
                                        "\"brightPurple\": \"#B4009E\","
                                        "\"brightRed\": \"#E74856\","
                                        "\"brightWhite\": \"#F2F2F2\","
                                        "\"brightYellow\": \"#F9F1A5\","
                                        "\"cursorColor\": \"#FFFFFF\","
                                        "\"cyan\": \"#3A96DD\","
                                        "\"foreground\": \"#F2F2F2\","
                                        "\"green\": \"#13A10E\","
                                        "\"name\": \"Campbell\","
                                        "\"purple\": \"#881798\","
                                        "\"red\": \"#C50F1F\","
                                        "\"selectionBackground\": \"#131313\","
                                        "\"white\": \"#CCCCCC\","
                                        "\"yellow\": \"#C19C00\""
                                        "}" };

        const auto json{ VerifyParseSucceeded(schemeString) };
        const auto scheme{ implementation::ColorScheme::FromJson(json) };

        const auto result{ scheme->ToJson() };
        VERIFY_ARE_EQUAL(schemeString, toString(result));
    }

    void SerializationTests::CascadiaSettings()
    {
        DebugBreak();
        // clang-format off
        // This needs to be in alphabetical order.
        const std::string settingsString{   "{"
                                                "\"$schema\": \"https://aka.ms/terminal-profiles-schema\","
                                                "\"actions\": ["
                                                    "{\"command\": {\"action\": \"renameTab\",\"input\": \"Liang Tab\"},\"keys\": \"ctrl+t\"}"
                                                "],"
                                                "\"defaultProfile\": \"{61c54bbd-1111-5271-96e7-009a87ff44bf}\","
                                                "\"keybindings\": ["
                                                    "{\"command\": {\"action\": \"sendInput\",\"input\": \"VT Griese Mode\"},\"keys\": \"ctrl+k\"}"
                                                "],"
                                                "\"profiles\": {"
                                                    "\"defaults\": {"
                                                        "\"fontFace\": \"Zamora Code\""
                                                    "},"
                                                    "\"list\": ["
                                                        "{"
                                                            "\"fontFace\": \"Cascadia Code\","
                                                            "\"guid\": \"{61c54bbd-1111-5271-96e7-009a87ff44bf}\","
                                                            "\"name\": \"HowettShell\""
                                                        "},"
                                                        "{"
                                                            "\"antialiasingMode\": \"aliased\","
                                                            "\"name\": \"NiksaShell\""
                                                        "},"
                                                        "{"
                                                            "\"name\": \"BhojwaniShell\","
                                                            "\"source\": \"local\""
                                                        "}"
                                                    "]"
                                                "},"
                                                "\"schemes\": ["
                                                    "{"
                                                        "\"background\": \"#3C0315\","
                                                        "\"black\": \"#282A2E\","
                                                        "\"blue\": \"#0170C5\","
                                                        "\"brightBlack\": \"#676E7A\","
                                                        "\"brightBlue\": \"#5C98C5\","
                                                        "\"brightCyan\": \"#8ABEB7\","
                                                        "\"brightGreen\": \"#B5D680\","
                                                        "\"brightPurple\": \"#AC79BB\","
                                                        "\"brightRed\": \"#BD6D85\","
                                                        "\"brightWhite\": \"#FFFFFD\","
                                                        "\"brightYellow\": \"#FFFD76\","
                                                        "\"cursorColor\": \"#FFFFFD\","
                                                        "\"cyan\": \"#3F8D83\","
                                                        "\"foreground\": \"#FFFFFD\","
                                                        "\"green\": \"#76AB23\","
                                                        "\"name\": \"Cinnamon Roll\","
                                                        "\"purple\": \"#7D498F\","
                                                        "\"red\": \"#BD0940\","
                                                        "\"selectionBackground\": \"#FFFFFF\","
                                                        "\"white\": \"#FFFFFD\","
                                                        "\"yellow\": \"#E0DE48\""
                                                    "}"
                                                "]"
                                            "}" };
        // clang-format on

        auto settings{ winrt::make_self<implementation::CascadiaSettings>(false) };
        settings->_ParseJsonString(settingsString, false);
        settings->_ApplyDefaultsFromUserSettings();
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        // TODO CARLOS:
        // - temp/DebugBreak is only for testing. Remove after we're done here.
        // - NiksaShell prints out a guid, when it shouldn't be
        // - BhojwaniShell is missing entirely
        const auto result{ settings->ToJson() };
        const auto temp{ toString(result) };
        VERIFY_ARE_EQUAL(settingsString, toString(result));
    }
}
