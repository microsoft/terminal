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
        DebugBreak();
        // This needs to be in alphabetical order.
        const std::string globalsString{
            "{"
            "\"alwaysOnTop\": false,"
            "\"alwaysShowTabs\": true,"
            "\"confirmCloseAllTabs\": true,"
            "\"copyFormatting\": \"all\","
            "\"copyOnSelect\": false,"
            "\"debugFeatures\": true,"
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
        DebugBreak();
        const std::string profileString{
            "{"
            "\"name\": \"Windows PowerShell\","
            "\"guid\": \"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}\","
            "\"hidden\": false,"
            "\"foreground\": \"#AABBCC\","
            "\"background\": \"#BBCCAA\","
            "\"selectionBackground\": \"#CCAABB\","
            "\"cursorColor\": \"#CCBBAA\","
            "\"colorScheme\": \"Campbell\","
            "\"historySize\": 9001,"
            "\"snapOnInput\": true,"
            "\"altGrAliasing\": true,"
            "\"cursorHeight\": null,"
            "\"cursorShape\": \"bar\","
            "\"tabTitle\": null,"
            "\"fontWeight\": \"normal\","
            //"\"connectionType\": null,"
            "\"commandline\": \"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe\","
            "\"fontFace\": \"Cascadia Mono\","
            "\"fontSize\": 12,"
            "\"useAcrylic\": false,"
            "\"suppressApplicationTitle\": false,"
            "\"closeOnExit\": \"graceful\","
            "\"padding\": \"8, 8, 8, 8\","
            "\"scrollbarState\": \"visible\","
            "\"startingDirectory\": \"%USERPROFILE%\","
            "\"icon\": \"ms-appx:///ProfileIcons/{61c54bbd-c2c6-5271-96e7-009a87ff44bf}.png\","
            "\"backgroundImage\": null,"
            "\"backgroundImageOpacity\": 1.0,"
            "\"backgroundImageStretchMode\": \"uniformToFill\","
            "\"backgroundImageAlignment\": \"center\","
            "\"experimental.retroTerminalEffect\": false,"
            "\"antialiasingMode\": \"grayscale\""
            "\"tabColor\": \"#0C0C0C\","
            "}"
        };

        const std::string smallProfileString{
            "{"
            "\"name\": \"Custom Profile\""
            "}"
        };

        auto roundtripTest = [this](auto jsonString) {
            const auto json{ VerifyParseSucceeded(jsonString) };
            const auto globals{ implementation::Profile::FromJson(json) };

            const auto result{ globals->ToJson() };

            VERIFY_ARE_EQUAL(jsonString, toString(result));
        };

        roundtripTest(profileString);
        roundtripTest(smallProfileString);
    }

    void SerializationTests::ColorScheme()
    {
        DebugBreak();
        const std::string schemeString{ "{"
                                        "\"name\" : \"Campbell\","
                                        "\"foreground\" : \"#F2F2F2\","
                                        "\"background\" : \"#0C0C0C\","
                                        "\"selectionBackground\" : \"#131313\","
                                        "\"cursorColor\" : \"#FFFFFF\","
                                        "\"black\" : \"#0C0C0C\","
                                        "\"red\" : \"#C50F1F\","
                                        "\"green\" : \"#13A10E\","
                                        "\"yellow\" : \"#C19C00\""
                                        "\"blue\" : \"#0037DA\","
                                        "\"purple\" : \"#881798\","
                                        "\"cyan\" : \"#3A96DD\","
                                        "\"white\" : \"#CCCCCC\","
                                        "\"brightBlack\" : \"#767676\","
                                        "\"brightRed\" : \"#E74856\","
                                        "\"brightGreen\" : \"#16C60C\","
                                        "\"brightYellow\" : \"#F9F1A5\","
                                        "\"brightBlue\" : \"#3B78FF\","
                                        "\"brightPurple\" : \"#B4009E\","
                                        "\"brightCyan\" : \"#61D6D6\","
                                        "\"brightWhite\" : \"#F2F2F2\""
                                        "}" };

        const auto json{ VerifyParseSucceeded(schemeString) };
        const auto profile{ implementation::ColorScheme::FromJson(json) };

        const auto result{ profile->ToJson() };
        VERIFY_ARE_EQUAL(json, result);
    }

    void SerializationTests::CascadiaSettings()
    {
        DebugBreak();
        const std::string settingsString{ "" };

        const auto json{ VerifyParseSucceeded(settingsString) };
        const auto settings{ implementation::CascadiaSettings::FromJson(json) };

        const auto result{ settings->ToJson() };
        VERIFY_ARE_EQUAL(json, result);
    }
}
