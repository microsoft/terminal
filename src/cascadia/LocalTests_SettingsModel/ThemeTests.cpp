// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/Theme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../types/inc/colorTable.hpp"
#include "JsonTestClass.h"

#include <defaults.h>

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class ThemeTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(ThemeTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(ParseSimpleTheme);
        TEST_METHOD(ParseThemeWithNullThemeColor);

        static Core::Color rgb(uint8_t r, uint8_t g, uint8_t b) noexcept
        {
            return Core::Color{ r, g, b, 255 };
        }
    };

    void ThemeTests::ParseSimpleTheme()
    {
        static constexpr std::string_view orangeTheme{ R"({
            "name": "orange",
            "tabRow":
            {
                "background": "#FFFF8800",
                "unfocusedBackground": "#FF884400"
            },
            "window":
            {
                "applicationTheme": "light",
                "useMica": true
            }
        })" };

        const auto schemeObject = VerifyParseSucceeded(orangeTheme);
        auto theme = Theme::FromJson(schemeObject);
        VERIFY_ARE_EQUAL(L"orange", theme->Name());

        VERIFY_IS_NOT_NULL(theme->TabRow());
        VERIFY_IS_NOT_NULL(theme->TabRow().Background());
        VERIFY_ARE_EQUAL(Settings::Model::ThemeColorType::Color, theme->TabRow().Background().ColorType());
        VERIFY_ARE_EQUAL(rgb(0xff, 0x88, 0x00), theme->TabRow().Background().Color());
    }

    void ThemeTests::ParseThemeWithNullThemeColor()
    {
        static constexpr std::string_view settingsString{ R"json({
            "themes": [
                {
                    "name": "backgroundEmpty",
                    "tabRow":
                    {
                    },
                    "window":
                    {
                        "applicationTheme": "light",
                        "useMica": true
                    }
                },
                {
                    "name": "backgroundNull",
                    "tabRow":
                    {
                        "background": null
                    },
                    "window":
                    {
                        "applicationTheme": "light",
                        "useMica": true
                    }
                },
                {
                    "name": "backgroundOmittedEntirely",
                    "window":
                    {
                        "applicationTheme": "light",
                        "useMica": true
                    }
                }
            ]
        })json" };

        try
        {
            const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, DefaultJson) };

            const auto& themes{ settings->GlobalSettings().Themes() };
            {
                const auto& backgroundEmpty{ themes.Lookup(L"backgroundEmpty") };
                VERIFY_ARE_EQUAL(L"backgroundEmpty", backgroundEmpty.Name());
                VERIFY_IS_NOT_NULL(backgroundEmpty.TabRow());
                VERIFY_IS_NULL(backgroundEmpty.TabRow().Background());
            }
            {
                const auto& backgroundNull{ themes.Lookup(L"backgroundNull") };
                VERIFY_ARE_EQUAL(L"backgroundNull", backgroundNull.Name());
                VERIFY_IS_NOT_NULL(backgroundNull.TabRow());
                VERIFY_IS_NULL(backgroundNull.TabRow().Background());
            }
            {
                const auto& backgroundOmittedEntirely{ themes.Lookup(L"backgroundOmittedEntirely") };
                VERIFY_ARE_EQUAL(L"backgroundOmittedEntirely", backgroundOmittedEntirely.Name());
                VERIFY_IS_NULL(backgroundOmittedEntirely.TabRow());
            }
        }
        catch (const SettingsException& ex)
        {
            auto loadError = ex.Error();
            loadError;
            DebugBreak();
            throw ex;
        }
        catch (const SettingsTypedDeserializationException& e)
        {
            auto deserializationErrorMessage = til::u8u16(e.what());
            Log::Comment(NoThrowString().Format(deserializationErrorMessage.c_str()));
            DebugBreak();
            throw e;
        }
    }
}
