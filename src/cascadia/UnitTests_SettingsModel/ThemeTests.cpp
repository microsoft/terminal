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

namespace SettingsModelUnitTests
{
    class ThemeTests : public JsonTestClass
    {
        TEST_CLASS(ThemeTests);

        TEST_METHOD(ParseSimpleTheme);
        TEST_METHOD(ParseEmptyTheme);
        TEST_METHOD(ParseNoWindowTheme);
        TEST_METHOD(ParseNullWindowTheme);
        TEST_METHOD(ParseThemeWithNullThemeColor);
        TEST_METHOD(InvalidCurrentTheme);

        static Core::Color rgb(uint8_t r, uint8_t g, uint8_t b) noexcept
        {
            return Core::Color{ r, g, b, 255 };
        }
        static Core::Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
        {
            return Core::Color{ r, g, b, a };
        }
    };

    void ThemeTests::ParseSimpleTheme()
    {
        static constexpr std::string_view orangeTheme{ R"({
            "name": "orange",
            "tabRow":
            {
                "background": "#FFFF8800",
                "unfocusedBackground": "#FF8844",
                "iconStyle": "default"
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
        VERIFY_ARE_EQUAL(rgba(0xff, 0xff, 0x88, 0x00), theme->TabRow().Background().Color());
        VERIFY_ARE_EQUAL(rgba(0xff, 0x88, 0x44, 0xff), theme->TabRow().UnfocusedBackground().Color());

        VERIFY_IS_NOT_NULL(theme->Window());
        VERIFY_ARE_EQUAL(winrt::Windows::UI::Xaml::ElementTheme::Light, theme->Window().RequestedTheme());
        VERIFY_ARE_EQUAL(true, theme->Window().UseMica());
    }

    void ThemeTests::ParseEmptyTheme()
    {
        Log::Comment(L"This theme doesn't have any elements defined.");
        static constexpr std::string_view emptyTheme{ R"({
            "name": "empty"
        })" };

        const auto schemeObject = VerifyParseSucceeded(emptyTheme);
        auto theme = Theme::FromJson(schemeObject);
        VERIFY_ARE_EQUAL(L"empty", theme->Name());
        VERIFY_IS_NULL(theme->TabRow());
        VERIFY_IS_NULL(theme->Window());
        VERIFY_ARE_EQUAL(winrt::Windows::UI::Xaml::ElementTheme::Default, theme->RequestedTheme());
    }

    void ThemeTests::ParseNoWindowTheme()
    {
        Log::Comment(L"This theme doesn't have a window defined.");
        static constexpr std::string_view emptyTheme{ R"({
            "name": "noWindow",
            "tabRow":
            {
                "background": "#112233",
                "unfocusedBackground": "#FF884400"
            },
        })" };

        const auto schemeObject = VerifyParseSucceeded(emptyTheme);
        auto theme = Theme::FromJson(schemeObject);
        VERIFY_ARE_EQUAL(L"noWindow", theme->Name());

        VERIFY_IS_NOT_NULL(theme->TabRow());
        VERIFY_IS_NOT_NULL(theme->TabRow().Background());
        VERIFY_ARE_EQUAL(Settings::Model::ThemeColorType::Color, theme->TabRow().Background().ColorType());
        VERIFY_ARE_EQUAL(rgb(0x11, 0x22, 0x33), theme->TabRow().Background().Color());

        VERIFY_IS_NULL(theme->Window());
        VERIFY_ARE_EQUAL(winrt::Windows::UI::Xaml::ElementTheme::Default, theme->RequestedTheme());
    }

    void ThemeTests::ParseNullWindowTheme()
    {
        Log::Comment(L"This theme doesn't have a window defined.");
        static constexpr std::string_view emptyTheme{ R"({
            "name": "nullWindow",
            "tabRow":
            {
                "background": "#112233",
                "unfocusedBackground": "#FF884400"
            },
            "window": null
        })" };

        const auto schemeObject = VerifyParseSucceeded(emptyTheme);
        auto theme = Theme::FromJson(schemeObject);
        VERIFY_ARE_EQUAL(L"nullWindow", theme->Name());

        VERIFY_IS_NOT_NULL(theme->TabRow());
        VERIFY_IS_NOT_NULL(theme->TabRow().Background());
        VERIFY_ARE_EQUAL(Settings::Model::ThemeColorType::Color, theme->TabRow().Background().ColorType());
        VERIFY_ARE_EQUAL(rgb(0x11, 0x22, 0x33), theme->TabRow().Background().Color());

        VERIFY_IS_NULL(theme->Window());
        VERIFY_ARE_EQUAL(winrt::Windows::UI::Xaml::ElementTheme::Default, theme->RequestedTheme());
    }

    void ThemeTests::ParseThemeWithNullThemeColor()
    {
        Log::Comment(L"These themes are all missing a tabRow background. Make sure we don't somehow default-construct one for them");

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
            throw ex;
        }
        catch (const SettingsTypedDeserializationException& e)
        {
            auto deserializationErrorMessage = til::u8u16(e.what());
            Log::Comment(NoThrowString().Format(deserializationErrorMessage.c_str()));
            throw e;
        }
    }

    void ThemeTests::InvalidCurrentTheme()
    {
        Log::Comment(L"Make sure specifying an invalid theme falls back to a sensible default.");

        static constexpr std::string_view settingsString{ R"json({
            "theme": "foo",
            "themes": [
                {
                    "name": "bar",
                    "tabRow": {},
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

            VERIFY_ARE_EQUAL(1u, settings->Warnings().Size());
            VERIFY_ARE_EQUAL(Settings::Model::SettingsLoadWarnings::UnknownTheme, settings->Warnings().GetAt(0));

            const auto& themes{ settings->GlobalSettings().Themes() };
            {
                const auto& bar{ themes.Lookup(L"bar") };
                VERIFY_ARE_EQUAL(L"bar", bar.Name());
                VERIFY_IS_NOT_NULL(bar.TabRow());
                VERIFY_IS_NULL(bar.TabRow().Background());
            }

            const auto currentTheme{ settings->GlobalSettings().CurrentTheme() };
            VERIFY_IS_NOT_NULL(currentTheme);
            VERIFY_ARE_EQUAL(L"system", currentTheme.Name());
        }
        catch (const SettingsException& ex)
        {
            auto loadError = ex.Error();
            loadError;
            throw ex;
        }
        catch (const SettingsTypedDeserializationException& e)
        {
            auto deserializationErrorMessage = til::u8u16(e.what());
            Log::Comment(NoThrowString().Format(deserializationErrorMessage.c_str()));
            throw e;
        }
    }
}
